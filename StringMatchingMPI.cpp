#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

long search(MPI_File *input,
			const char* target,
			const int targetSize,
			const int processID,
			const int numOfProc) {
    
	MPI_Offset offsetFrom, offsetTo, fileSize;
	char *chunk;
	int chunkSize;
	long numberOfMatches = 0;
	
    MPI_File_get_size(*input, &fileSize);
	fileSize--; 
		
    chunkSize = fileSize/numOfProc;
    
	offsetFrom = processID * chunkSize;
    offsetTo   = offsetFrom + chunkSize - 1;
    if (processID == numOfProc-1) offsetTo = fileSize;

    if (processID != numOfProc-1) offsetTo += targetSize;

    chunkSize =  offsetTo - offsetFrom + 1;

    /* allocate memory */
    chunk = (char *) malloc( (chunkSize + 1) * sizeof(char));

    /* everyone reads in their part */
    MPI_File_read_at_all(*input, offsetFrom, chunk, chunkSize, MPI_CHAR, MPI_STATUS_IGNORE);
    
	chunk[chunkSize] = '\0';

	//printf("ChunkSize=%d Processor#%d : %s\n\n", chunkSize, processID, chunk);
	
	for(int i=0; i<chunkSize-targetSize; i++){
		if(chunk[i]==target[0]){
			int flag = 1;
            for(int j = 1; j < targetSize; j++){
                if(chunk[i+j] != target[j]){
                    flag = 0; break;
                }
            }
            
            if(flag == 1){
                printf("**Match at position %llu\n", offsetFrom + i);
				numberOfMatches++;
            }
		}
	}
	
    return numberOfMatches;
}

int main(int argc, char **argv) {

    MPI_File original;
	char* target;
    int rank, size, error;
	double wtime;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

	if(rank == 0){
		wtime = MPI_Wtime();
	}
	
    if (argc != 3) {
        if (rank == 0) fprintf(stderr, "Usage: %s originalTextFile targetWord\n", argv[0]);
        MPI_Finalize();
        exit(1);
    }

    error = MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_RDONLY, MPI_INFO_NULL, &original);
    if (error) {
        if (rank == 0) fprintf(stderr, "%s: Couldn't open file %s\n", argv[0], argv[1]);
        MPI_Finalize();
        exit(2);
    }

	target = argv[2];
	const long unsigned targetSize = strlen(target);
	long totalMatches = 0;
	
	if(rank == 0)
		printf("Searching for:%s\n", target);
	
	MPI_Barrier(MPI_COMM_WORLD);
    
	totalMatches = search(&original, target, targetSize, rank, size);
	
	MPI_Barrier(MPI_COMM_WORLD);
	
	if(rank != 0){
		MPI_Send(&totalMatches, 1, MPI_LONG, 0, rank, MPI_COMM_WORLD);
	}else{
		for(int process = 1; process < size; process++){
			long receive = 0;
			MPI_Status status;
			MPI_Recv(&receive, 1, MPI_LONG, process, process, MPI_COMM_WORLD, &status);
			totalMatches += receive;
		}
		
		printf("Total number of matches: %lu\n\n", totalMatches);
		printf("Total elapsed time: %fms\n\n", (MPI_Wtime() - wtime)*1000);
	}
	
    MPI_File_close(&original);
    MPI_Finalize();

    return 0;
}
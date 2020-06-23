#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int minSkips = 0;
int maxSkips = 40;



int main(){

  
  //int numDirs = rand() % (maxNumDirs - minNumDirs + 1);
  //int numFiles = rand() % (maxNumFiles - minNumFiles + 1);
  //int depth = rand() % (maxTreeDepth - minTreeDepth +1);
  //int depth = maxTreeDepth;
  
  //buildTree(&root, depth, numDirs, numFiles);

  FILE * out;
  out = fopen("workload.txt", "w");
  if( out == NULL) {
    perror("Error opening output file\n");
    return -1;
  }
  FILE * in;
  in = fopen("filesystem.txt", "r");
  if( in == NULL) {
    perror("Error opening input file\n");
    return -1;
  }
  
  char * nextline = (char*) malloc(sizeof(char)*450);
  int numSkips;
  
  while(!feof(in)){
    numSkips = rand() % (maxSkips - minSkips + 1);
    for(int i = 0; i<numSkips; i++){
      fgets(nextline, 450, in);
    }
    printf("Skips = %d\n", numSkips);
    fprintf(out, "%s", nextline);
  }

  fclose(in);
  fclose(out);

  return 0;
  
}

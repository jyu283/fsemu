#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

int minTreeDepth = 1;
int maxTreeDepth = 20;
int minNumFiles = 1;
int maxNumFiles = 10;
int minNumDirs = 1;
int maxNumDirs = 10;
int minFileNameLength = 2;
int maxFileNameLength = 20;
int minDirNameLength = 3;
int maxDirNameLength = 20;


int narrowMinWidth = 1;
int narrowMaxWidth = 10;
int moderateMinWidth = 5;
int moderateMaxWidth = 15;
int wideMinWidth = 10;
int wideMaxWidth = 20;

int shortMinName = 2;
int shortMaxName = 10;
int moderateMinName = 5;
int moderateMaxName = 15;
int longMinName = 10;
int longMaxName = 20;

const char* fileNames[] = {"a", "uw", "log", "file", "trees", "flower", "project", "download", "documents", "photograph", "foobashbash", "bashbashbash", "foobarbazbash", "foobarbazzbash", "foobarbazfoobar", "starwarsanewhope", "empirestrikesback", "swrevengeofthesith", "c3por2d2bb8l3atat!-"/* ... etc ... */ };
int fileNameUsage[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char* extNames[] = {".c", ".py", ".txt"};
const char* dirNames[] = {"uw", "log", "ewok", "users", "recent", "project", "download", "documents", "photograph", "development", "peanutbutter", "constellation", "antidepressant", "thefinalfrontier", "milleniumfalcon", "thunder&lightning", "unsatisfactoriness", "inagalaxyfarfaraway"};
int dirNameUsage[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


typedef struct file file;
struct file{
  char * name;
  int length;
  file * next;
};

typedef struct dir dir;
struct dir{
  char * name;
  int length;
  dir * dirs;
  file * files;
  dir * next;
};


/*typedef struct{
  dir * root;
} tree;
*/

//print tree
int printTree (dir * Dir, char* path, FILE * out){
  //print curr dir
  fprintf( out, "D %s%s/\n", path, Dir->name);
  //print curr directory files
  file * curr = Dir->files;
  while (curr != NULL){
    fprintf( out , "F %s%s/%s\n", path, Dir->name, curr->name);
    curr = curr->next;
  }
  //for loop to recursively print directories
  dir * currD = Dir->dirs;
  char * newpath = (char*) malloc(strlen(path)+strlen(Dir->name)+5);
  sprintf( newpath, "%s%s/", path, Dir->name);
  while (currD != NULL){
    printTree( currD, newpath, out);
    currD = currD->next;
  }
  return 0;
}


void fileBuildTree(dir * parent, FILE * in){



}


void uniformBuildTree (dir* parent, int depth, int numDirs, int numFiles, int minName, int maxName){
  printf("currD = %s; depth = %d; numdir = %d; numfile = %d\n", parent->name, depth, numDirs, numFiles);
  if(numFiles == 0){
    parent->files = NULL;
  }else{
    parent->files = (struct file*)malloc(sizeof(file));
    parent->files->next = NULL;
  }

  if(numDirs == 0){
    parent->dirs = NULL;
  }else{
    parent->dirs = (struct dir*)malloc(sizeof(dir));
    parent->dirs->next = NULL;
  }
  //Files loop
  file* curr = parent->files;
  
  for(int i = 0; i < numFiles; i++){
    int nameNum = rand() % (maxName - minName + 1);
    int nameLen = nameNum+minFileNameLength+1;

    if (i == 0){
      curr->name= (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      curr->length = sizeof(curr->name);
    }
    
    else{
      file* new = (struct file*)malloc(sizeof(file));
      curr->next = new;
      curr = curr->next;
      curr->name = (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      curr->length = sizeof(curr->name);
      curr->next = NULL;
    }
  }

  if (depth == 0){
    return;
  }
  //Directory Loop
  dir* currD = parent->dirs;
  
  for(int i = 0; i < numDirs; i++){
    int nameNum = rand() % (maxName - minName + 1);
    int nameLen = nameNum+minDirNameLength+1;
    if (i == 0){
      currD->name= (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      currD->length = sizeof(currD->name);
    }
    else{
      dir* new = (struct dir*)malloc(sizeof(dir));
      currD->next = new;
      currD = currD->next;
      currD->name = (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      currD->length = sizeof(currD->name);
      currD->next = NULL;
    }
    int nDirs;
    int newDepth = depth - 1;
    if(newDepth == 1){
      nDirs = 0;
    }else{
      nDirs = numDirs;
    }
    //printf("newDepth = %d\n", newDepth);
    uniformBuildTree(currD, newDepth, nDirs, numFiles, minName, maxName);
  }
  return;
}



void buildTree (dir* parent, int depth, int numDirs, int numFiles,int minWidth, int maxWidth, int minName, int maxName){
  printf("currD = %s; depth = %d; numdir = %d; numfile = %d\n", parent->name, depth, numDirs, numFiles);
  if(numFiles == 0){
    parent->files = NULL;
  }else{
    parent->files = (struct file*)malloc(sizeof(file));
    parent->files->next = NULL;
  }

  if(numDirs == 0){
    parent->dirs = NULL;
  }else{
    parent->dirs = (struct dir*)malloc(sizeof(dir));
    parent->dirs->next = NULL;
  }
  //Files loop
  file* curr = parent->files;
  
  for(int i = 0; i < numFiles; i++){
    int nameNum = rand() % (maxName - minName + 1);
    int nameLen = nameNum+minFileNameLength+1;

    if (i == 0){
      curr->name= (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      curr->length = sizeof(curr->name);
    }
    
    else{
      file* new = (struct file*)malloc(sizeof(file));
      curr->next = new;
      curr = curr->next;
      curr->name = (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      curr->length = sizeof(curr->name);
      curr->next = NULL;
    }
  }

  if (depth == 0){
    return;
  }
  //Directory Loop
  dir* currD = parent->dirs;
  
  for(int i = 0; i < numDirs; i++){
    int nameNum = rand() % (maxName - minName + 1);
    int nameLen = nameNum+minDirNameLength+1;
    if (i == 0){
      currD->name= (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      currD->length = sizeof(currD->name);
    }
    else{
      dir* new = (struct dir*)malloc(sizeof(dir));
      currD->next = new;
      currD = currD->next;
      currD->name = (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      currD->length = sizeof(currD->name);
      currD->next = NULL;
    }
    int nDirs = rand() % (maxWidth - minWidth + 1);
    int nFiles = rand() % (maxWidth - minWidth + 1);
    int newDepth;
    if(depth != 1){
      newDepth = rand() % (depth-1);
    }else{
      newDepth = 0;
    }
    if(newDepth == 0){
      nDirs = 0;
    }
    printf("newDepth = %d\n", newDepth);
    buildTree(currD, newDepth, nDirs, nFiles, minWidth, maxWidth, minName, maxName);
  }
  return;
}



void defaultBuildTree (dir* parent, int depth, int numDirs, int numFiles){
  //printf("currD = %s; depth = %d; numdir = %d; numfile = %d\n", parent->name, depth, numDirs, numFiles);
  if(numFiles == 0){
    parent->files = NULL;
  }else{
    parent->files = (struct file*)malloc(sizeof(file));
    parent->files->next = NULL;
  }

  if(numDirs == 0){
    parent->dirs = NULL;
  }else{
    parent->dirs = (struct dir*)malloc(sizeof(dir));
    parent->dirs->next = NULL;
  }
  //Files loop
  file* curr = parent->files;
  
  for(int i = 0; i < numFiles; i++){
    int nameNum = rand() % (maxFileNameLength - minFileNameLength + 1);
    int nameLen = nameNum+minFileNameLength+1;

    if (i == 0){
      curr->name= (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      curr->length = sizeof(curr->name);
    }
    
    else{
      file* new = (struct file*)malloc(sizeof(file));
      curr->next = new;
      curr = curr->next;
      curr->name = (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      curr->length = sizeof(curr->name);
      curr->next = NULL;
    }
  }

  if (depth == 0){
    return;
  }
  //Directory Loop
  dir* currD = parent->dirs;
  
  for(int i = 0; i < numDirs; i++){
    int nameNum = rand() % (maxDirNameLength - minDirNameLength + 1);
    int nameLen = nameNum+minDirNameLength+1;
    if (i == 0){
      currD->name= (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      currD->length = sizeof(currD->name);
    }
    else{
      dir* new = (struct dir*)malloc(sizeof(dir));
      currD->next = new;
      currD = currD->next;
      currD->name = (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      currD->length = sizeof(currD->name);
      currD->next = NULL;
    }
    int nDirs = rand() % (maxNumDirs - minNumDirs + 1);
    int nFiles = rand() % (maxNumFiles - minNumFiles + 1);
    int newDepth;
    if(depth != 1){
      newDepth = rand() % (depth-1);
    }else{
      newDepth = 0;
    }
    if(newDepth == 0){
      nDirs = 0;
    }
    printf("newDepth = %d\n", newDepth);
    defaultBuildTree(currD, newDepth, nDirs, nFiles);
  }
  return;
}

int main(int argc, char *argv[]){

  //example program calls:
  //./fsgen -B minD= # or R maxD= # or R  Width= WidthCode Names= NameCode Form= FormCode
  //Width codes = N{Narrow}, W{Wide}, M{Moderate}, R{Random}
  //Name codes = S{Short}, L{Long}, M{Moderate}, R{Random}
  //Form Codes = U{Uniform}, R{Random}
  //
  //./fsgen -B minD= 1 maxD= 20 Width= N Names= L Form= U -W {workload options}
  //./fsgen -F test.txt -W {workload options}

  
  dir root;
  root.name = "root";
  root.length = 4;

  if(argc == 1){
    int numDirs = rand() % (maxNumDirs - minNumDirs + 1);
    int numFiles = rand() % (maxNumFiles - minNumFiles + 1);
    //int depth = maxTreeDepth;
    int depth = rand() % (maxTreeDepth - minTreeDepth + 1);
    
    defaultBuildTree(&root, depth, numDirs, numFiles);
  }
  //int nextArg = 1;
  else{
    int nextArg = 1;
    bool isUniform;
    int minDepth;
    int maxDepth;
    int minWidth;
    int maxWidth;
    int minName;
    int maxName;
    int depth;
    int numDirs;
    int numFiles;
    if(strcmp(argv[nextArg], "-B") == 0){
      nextArg = 12;
      if(argc < 10){
	printf("Not enough arguments for building a new tree\n");
	return -1;
      }
      for(int i = 2; i<12; i = i+2){
	if(strcmp(argv[i], "minD=") == 0){
	  if(strcmp(argv[i+1], "R") == 0){
	    minDepth = minTreeDepth;
	  }else{
	    minDepth = atoi(argv[i+1]);
	  }
	}else if(strcmp(argv[i], "maxD=") == 0){
	  if(strcmp(argv[i+1], "R") == 0){
            maxDepth = maxTreeDepth;
          }else{
            maxDepth = atoi(argv[i+1]);
          }
	}else if(strcmp(argv[i], "Width=") == 0){
	  if(strcmp(argv[i+1], "N") == 0){
	    minWidth = narrowMinWidth;
	    maxWidth = narrowMaxWidth;
          }else if(strcmp(argv[i+1], "M") == 0){
	    minWidth = moderateMinWidth;
	    maxWidth = moderateMaxWidth;
          }else if(strcmp(argv[i+1], "W") == 0){
	    minWidth = wideMinWidth;
	    maxWidth = wideMaxWidth;
          }else if(strcmp(argv[i+1], "R") == 0){
	    minWidth = narrowMinWidth;
	    maxWidth = wideMaxWidth;
          }else{
	    printf("Invalid Width Code\n");
	    return -1;
	  }
	}else if(strcmp(argv[i], "Names=") == 0){
	  if(strcmp(argv[i+1], "S") == 0){
	    minName = shortMinName;
	    maxName = shortMaxName;
          }else if(strcmp(argv[i+1], "M") == 0){
	    minName = moderateMinName;
            maxName = moderateMaxName;
          }else if(strcmp(argv[i+1], "L") == 0){
	    minName = longMinName;
            maxName = longMaxName;
          }else if(strcmp(argv[i+1], "R") == 0){
	    minName = shortMinName;
            maxName = longMaxName;
          }else{
            printf("Invalid Name Code\n");
            return -1;
          }
	}else if(strcmp(argv[i], "Form=") == 0){
	  if(strcmp(argv[i+1], "U") == 0){
	    isUniform = true;
          }else if(strcmp(argv[i+1], "R") == 0){
	    isUniform = false;
          }else{
            printf("Invalid Width Code\n");
            return -1;
          }
	}else{
	  printf( "Unknown argument format\n");
	  return -1;
	}
      }

      if(minDepth > maxDepth){
	printf("Min Depth is larger that max Depth\n");
	return -1;
      }else if(minDepth == maxDepth){
	depth = minDepth;
      }else{
	depth = rand() % (maxDepth - minDepth + 1);
      }

      numDirs = rand() % (maxWidth - minWidth + 1);
      numFiles = rand() % (maxWidth - minWidth + 1);
      if(isUniform){
	uniformBuildTree(&root, depth, numDirs, numFiles, minName, maxName);
      }else{
	buildTree(&root, depth, numDirs, numFiles, minWidth, maxWidth, minName, maxName);
      }
    }

    else if(strcmp(argv[nextArg], "-F") == 0){
      if(argc < 3){
	printf("Not enough argument for building a tree from a file\n");
	return -1;
      }
      //char * filename = argv[nextArg + 1];
      printf( "%s\n", argv[nextArg + 1]);
      FILE * in;
      in = fopen(argv[nextArg + 1], "r");
      if( in == NULL ){
	printf("Could not open file containing filesystem tree\n");
	return -1;
      }

      char c = fgetc(in);

      while(c != '/'){
	c = fgetc(in);
      }
      root.name = (char *)malloc(sizeof(char)*25);
      //keep track of name length?

      c = fgetc(in);
      while(c != '/'){
	strncat(root.name, &c, 1);
	c = fgetc(in);
      }

      fileBuildTree(&root, in);

      nextArg = 3;
    }
    
    else{
      printf("INVALID first argument\n");
      return -1;
    }

    
    if(argc > nextArg){
      if(strcmp(argv[nextArg], "-W") == 0){
	//if(argc < ??){
	//printf("Not enough arguments to generate a workload\n");
	//return -2;
	//}
	//parse workload arguments
	//call workload generate function
      }
    }
  }
      
  FILE * out;
  out = fopen("filesystem.txt", "w");
  if( out == NULL) {
    return -1;
  }
  char * path = "/";
  printTree(&root, path, out);


  return 0;
  
}

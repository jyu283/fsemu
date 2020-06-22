#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int minTreeDepth = 1;
int maxTreeDepth = 4;
int minNumFiles = 1;
int maxNumFiles = 20;
int minNumDirs = 1;
int maxNumDirs = 20;
int minFileNameLength = 2;
int maxFileNameLength = 20;
int minDirNameLength = 3;
int maxDirNameLength = 20;

/*struct level{
  int level;
  entry * firstEntry;
  level * nextLevel;
};
*/

const char* fileNames[] = {"a", "uw", "log", "file", "trees", "flower", "project", "download", "documents", "photograph", "foobashbash", "bashbashbash", "foobarbazbash", "foobarbazzbash", "foobarbazfoobar", "starwarsanewhope", "empirestrikesback", "swrevengeofthesith", "c3por2d2bb8l3atat!-"/* ... etc ... */ };
int* fileNameUsage[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char* extNames[] = {".c", ".py", ".txt"};
const char* dirNames[] = {"uw", "log", "ewok", "users", "recent", "project", "download", "documents", "photograph", "development", "peanutbutter", "constellation", "antidepressant", "thefinalfrontier", "milleniumfalcon", "thunder&lightning", "unsatisfactoriness", "inagalaxyfarfaraway"};
int* dirNameUsage[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


typedef struct file file;
struct file{
  char * name;
  int length;
  file * next;
};

typedef struct dir dir;
struct dir{
  //int depth;
  char * name;
  int length;
  //union entry * firstEntry;
  dir * dirs;
  file * files;
  dir * next;
};


/*union entry{
  dir d;
  file f;
  };*/

/*struct entry{
  char type;
  entry *  nextEntry;
  char * name;
  char * length;
  };*/



typedef struct{
  dir * root;
} tree;


//print tree
int printTree (dir * Dir, char* path, FILE * out){
  //print curr dir
  fprintf( out, "D\t%s%s/\n", path, Dir->name);
  //print curr directory files
  file * curr = Dir->files;
  while (curr != NULL){
    fprintf( out , "F\t%s%s/%s\n", path, Dir->name, curr->name);
    curr = curr->next;
  }
  //for loop to recursively print directories
  dir * currD = Dir->dirs;
  char * newpath = (char*) malloc(sizeof(path)+sizeof(Dir->name)+1);
  sprintf( newpath, "%s%s/", path, Dir->name);
  while (currD != NULL){
    //fprintf( out, "%s%s/%s/\n", path, dir->name, currD->name);
    //char * newpath = (char*) malloc(sizeof(path)+sizeof(Dir->name)+1);
    //sprintf( newpath, "%s/%s", path, dir->name);
    printTree( currD, newpath, out);
    currD = currD->next;
  }
  return 0;
}

void buildTree (dir* parent, int depth, int numDirs, int numFiles){
  //allocate files
  //curr->name = name;
  //curr->length = length;
  //parent->files->name = "";
  //parent->files->length = 0;
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
    int nameNum = rand() % (maxFileNameLength - minFileNameLength + 1);
    int nameLen = nameNum+minFileNameLength+1;

    if (i == 0){
      //file* new = (struct file*)malloc(sizeof(file));
      //char * name
      curr->name= (char*) malloc(sizeof(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      //new->name = (char*) malloc((nameLen)*sizeof(char));
      //new->name = fileNames[nameNum];
      curr->length = sizeof(curr->name);
    }
    
    else{
      file* new = (struct file*)malloc(sizeof(file));
      curr->next = new;
      curr = curr->next;
      curr->name = (char*) malloc(sizeof(fileNames[nameNum])+(sizeof(int)*8+1));
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
  //allocate dirs
    int nameNum = rand() % (maxDirNameLength - minDirNameLength + 1);
    int nameLen = nameNum+minDirNameLength+1;
  //link dirs, name dirs
    if (i == 0){
      currD->name= (char*) malloc(sizeof(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      currD->length = sizeof(currD->name);
    }
    else{
      dir* new = (struct dir*)malloc(sizeof(dir));
      currD->next = new;
      currD = currD->next;
      currD->name = (char*) malloc(sizeof(dirNames[nameNum])+(sizeof(int)*8+1));
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
    buildTree(currD, newDepth, nDirs, nFiles);
  }
  return;
}

int main(){

  
  tree filesystem;

  filesystem.root->name = "root";
  filesystem.root->length = 4;
  int numDirs = rand() % (maxNumDirs - minNumDirs + 1);
  int numFiles = rand() % (maxNumFiles - minNumFiles + 1);
  //int depth = rand() % (maxTreeDepth - minTreeDepth +1);
  int depth = maxTreeDepth;
  
  buildTree(filesystem.root, depth, numDirs, numFiles);

  FILE * out;
  out = fopen("filesystem.txt", "w");
  if( out == NULL) {
    return -1;
  }

  printTree(filesystem.root, "", out);


  return 0;
  
}

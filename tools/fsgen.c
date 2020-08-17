#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>

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
int narrowMaxWidth = 5;
int moderateMinWidth = 5;
int moderateMaxWidth = 10;
int wideMinWidth = 10;
int wideMaxWidth = 15;

int shortMinName = 2;
int shortMaxName = 10;
int moderateMinName = 5;
int moderateMaxName = 15;
int longMinName = 10;
int longMaxName = 20;


int maxChosenDepth;




const char* fileNames[] = {"a", "uw", "log", "file", "trees", "flower", "project", "download", "documents", "photograph", "foobashbash", "bashbashbash", "foobarbazbash", "foobarbazzbash", "foobarbazfoobar", "starwarsanewhope", "empirestrikesback", "swrevengeofthesith", "c3por2d2bb8l3atat!-"/* ... etc ... */ };
int fileNameUsage[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char* extNames[] = {".c", ".py", ".txt"};
const char* dirNames[] = {"uw", "log", "ewok", "users", "recent", "project", "download", "documents", "photograph", "development", "peanutbutter", "constellation", "antidepressant", "thefinalfrontier", "milleniumfalcon", "thunder&lightning", "unsatisfactoriness", "inagalaxyfarfaraway"};
int dirNameUsage[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


typedef struct file file;
struct file{
  char * name;
  //int length;
  file * next;
};

typedef struct dir dir;
struct dir{
  char * name;
  //int length;
  dir * depthNext;
  dir * parent;
  dir * dirs;
  file * files;
  dir * next;
  int whichDepth;  //max - what you think it is
};

typedef struct depthInfo depthInfo;
struct depthInfo{
  dir * first;
  int num;
};


//really shouldn't be a global variable
//depth 1 = root
//don't use depth 0
//example. for depth 2 there will be root, files and dirs in root and files in those dirs; root is depth 1 and the children directories are depth 2
depthInfo * depthInfoArray = NULL;

void addToDepth(int whichDepth, dir* dir){
  assert (whichDepth < maxTreeDepth);
  assert(depthInfoArray);

  dir->depthNext =   depthInfoArray[whichDepth].first;
  depthInfoArray[whichDepth].first = dir;
  depthInfoArray[whichDepth].num++;
}

/*typedef struct{
  dir * root;
} tree;
*/

//print tree
int printTree (dir * Dir, char* path, FILE * out){
  //printf("In print tree\n");
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


void fileBuildTree(dir * parent, FILE * in, int parentNameLength, char * path, int depth){
  parent->files = NULL;
  parent->dirs = NULL;

  file * currf = NULL;// = parent->files;
  dir * currd = NULL;// = parent->dirs;
  
  addToDepth(depth, parent);


  while(!feof(in)){
    //read in new name
    char * parentPath = (char *)malloc(sizeof(char)*parentNameLength);
    char * prefix = (char *) malloc(sizeof(char) * 2);
    char c;
    long int currPos = ftell(in);
    for(int i = 0; i < parentNameLength; i++){
      c = fgetc(in);
      if( i > 1){
	strncat(parentPath, &c, 1);
      }else{
	strncat(prefix, &c, 1);
      }
    }

    //printf("%s\n", parentPath);
    //printf("%s\n", path);
    
    if(strcmp(parentPath, path) != 0){
      fseek(in, currPos, SEEK_SET);
      fwrite(prefix, sizeof(char), 2, in);
      fwrite(parentPath, sizeof(char), sizeof(parentPath), in);
      //printf("prefix: %s\n", prefix);
      //printf("path: %s\n", parentPath);
      fseek(in, currPos, SEEK_SET);
      return;
    }
    //printf("before name building\n");
    char * name = (char *)malloc(sizeof(char)*25);
    int nameLength = parentNameLength;                                                                                 
    //build name                                                                                                      
    c = fgetc(in);
    while(c != '/' && c != '\n'){
      //printf("%s\n", name);
      nameLength++;
      strncat(name, &c, 1);
      c = fgetc(in);
    }

    //printf("Got the name\n");
    //if file
    if( c == '\n'){
      //printf("file\n");
      //allocate file to parent and fill
      if(parent->files == NULL){
	parent->files = (struct file*)malloc(sizeof(file));
	parent->files->next = NULL;
	currf = parent->files;
	currf->name = (char*) malloc(strlen(name));
	sprintf(currf->name, "%s", name);
	//currf->length = sizeof(currf->name);
	//currf->next = NULL;
      }
      else{
	file * newF = (struct file*)malloc(sizeof(file));
	assert(newF);

	currf->next = newF;
	currf = currf->next;
	currf->name = (char*) malloc(strlen(name));
	sprintf(currf->name, "%s", name);
	//currf->length = sizeof(currf->name);
	currf->next = NULL;
      }
    }
    //if dir
    else if(c == '/'){
      //printf("dir\n");
      //increment name Length
      nameLength++;
      //grab newline
      c = fgetc(in);
      /*if(c == '\n'){                                                                                                 
	printf("Success in grabing newline\n");                                                                        
	}*/
      //allocate dir to parent and fill
      if(parent->dirs == NULL){
	//printf("first new dir\n");
        parent->dirs = (struct dir*)malloc(sizeof(dir));
        parent->dirs->next = NULL;
	parent->dirs->parent = parent;
        currd = parent->dirs;
        currd->name = (char*) malloc(strlen(name));
        sprintf(currd->name, "%s", name);
        //currd->length = sizeof(currd->name);
        currd->next = NULL;
	currd->whichDepth = depth + 1;
      }
      else{
	//printf("NEXT DIR\n");
        dir * newD = (struct dir*)malloc(sizeof(dir));
	assert(newD);

        currd->next = newD;
        currd = currd->next;
        currd->name = (char*) malloc(strlen(name));
	currd->parent = parent;
        sprintf(currd->name, "%s", name);
        //currd->length = sizeof(currd->name);
        currd->next = NULL;
	currd->whichDepth = depth + 1;
      }
 
      //recurse
      char * newPath = (char*)malloc((strlen(name)+parentNameLength)*sizeof(char));
      sprintf(newPath, "%s%s/",path, name); 
      fileBuildTree(currd, in, nameLength, newPath, depth+1);
      //currd = currd->next;
    }
    else{
      printf("Error\n");
      return;
    }
  }
  return;
}


void uniformBuildTree (dir* parent, int depth, int numDirs, int numFiles, int minName, int maxName){
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
    parent->dirs->parent = parent;
  }

  parent->whichDepth = maxChosenDepth - depth;

  addToDepth(maxChosenDepth - depth, parent);   

  //Files loop
  file* curr = parent->files;


  for(int i = 0; i < numFiles; i++){
    int nameNum = rand() % (maxName - minName + 1) + minName;
    int nameLen = nameNum+minFileNameLength+1;

    if (i == 0){
      curr->name= (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      //curr->length = sizeof(curr->name);
    }
    
    else{
      file* newF = (struct file*)malloc(sizeof(file));
      assert(newF);

      curr->next = newF;
      curr = curr->next;
      curr->name = (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      //curr->length = sizeof(curr->name);
      curr->next = NULL;
    }
  }

  if (depth == 0){
    return;
  }
  //Directory Loop
  dir* currD = parent->dirs;
  
  for(int i = 0; i < numDirs; i++){

    //int childDepth = maxChosenDepth - depth +1;
    //parent is at maxChosenDepth - depth 
    //child is at maxChosenDepth - depth +1 

    int nameNum = rand() % (maxName - minName + 1) + minName;
    int nameLen = nameNum+minDirNameLength+1;
    if (i == 0){
      currD->name= (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      //currD->length = sizeof(currD->name);
      //addToDepth(childDepth, currD);
    }
    else{
      dir* newD = (struct dir*)malloc(sizeof(dir));
      assert(newD);

      currD->next = newD;
      currD = currD->next;
      currD->name = (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      currD->parent = parent;
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      //currD->length = sizeof(currD->name);
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



void buildTree (dir* parent, int depth, int numDirs, int numFiles,int minWidth, int maxWidth, int minName, int maxName, int depthFromRoot){
  printf("currD = %s; depth = %d; numdir = %d; numfile = %d\n", parent->name, depth, numDirs, numFiles);

  addToDepth(depthFromRoot, parent);
  depthFromRoot++;

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
    parent->dirs->parent = parent;
  }
  //Files loop
  file* curr = parent->files;
  
  for(int i = 0; i < numFiles; i++){
    int nameNum = rand() % (maxName - minName + 1) + minName;
    int nameLen = nameNum+minFileNameLength+1;

    if (i == 0){
      curr->name= (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      //curr->length = sizeof(curr->name);
    }
    
    else{
      file* newF = (struct file*)malloc(sizeof(file));
      assert(newF);

      curr->next = newF;
      curr = curr->next;
      curr->name = (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      //curr->length = sizeof(curr->name);
      curr->next = NULL;
    }
  }

  if (depth == 0){
    return;
  }
  //Directory Loop
  dir* currD = parent->dirs;
  
  for(int i = 0; i < numDirs; i++){
    int nameNum = rand() % (maxName - minName + 1) + minName;
    int nameLen = nameNum+minDirNameLength+1;
    if (i == 0){
      currD->name= (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      //currD->length = sizeof(currD->name);
    }
    else{
      dir* newD = (struct dir*)malloc(sizeof(dir));
      assert(newD);

      currD->next = newD;
      currD = currD->next;
      currD->name = (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      currD->parent = parent;

      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      //currD->length = sizeof(currD->name);
      currD->next = NULL;
    }
    int nDirs = rand() % (maxWidth - minWidth + 1) + minWidth;
    int nFiles = rand() % (maxWidth - minWidth + 1) + minWidth;
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
    buildTree(currD, newDepth, nDirs, nFiles, minWidth, maxWidth, minName, maxName, depthFromRoot);
  }
  return;
}



void defaultBuildTree (dir* parent, int depth, int numDirs, int numFiles, int depthFromRoot){
  //printf("currD = %s; depth = %d; numdir = %d; numfile = %d\n", parent->name, depth, numDirs, numFiles);

  addToDepth(depthFromRoot, parent);
  depthFromRoot++;

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
    parent->dirs->parent = parent;
  }
  //Files loop
  file* curr = parent->files;
  
  for(int i = 0; i < numFiles; i++){
    int nameNum = rand() % (maxFileNameLength - minFileNameLength + 1) + minFileNameLength;
    int nameLen = nameNum+minFileNameLength+1;

    if (i == 0){
      curr->name= (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      //curr->length = sizeof(curr->name);
    }
    
    else{
      file* newF = (struct file*)malloc(sizeof(file));
      assert(newF);

      curr->next = newF;
      curr = curr->next;
      curr->name = (char*) malloc(strlen(fileNames[nameNum])+(sizeof(int)*8+1));
      sprintf(curr->name, "%s%d", fileNames[nameNum], fileNameUsage[nameNum]);
      fileNameUsage[nameNum]++;
      //curr->length = sizeof(curr->name);
      curr->next = NULL;
    }
  }

  if (depth == 0){
    return;
  }
  //Directory Loop
  dir* currD = parent->dirs;
  
  for(int i = 0; i < numDirs; i++){
    int nameNum = rand() % (maxDirNameLength - minDirNameLength + 1) + minDirNameLength;
    int nameLen = nameNum+minDirNameLength+1;
    if (i == 0){
      currD->name= (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      //currD->length = sizeof(currD->name);
    }
    else{
      dir* newD = (struct dir*)malloc(sizeof(dir));
      assert(newD);

      currD->next = newD;
      currD = currD->next;
      currD->name = (char*) malloc(strlen(dirNames[nameNum])+(sizeof(int)*8+1));
      currD->parent = parent;

      sprintf(currD->name, "%s%d", dirNames[nameNum], dirNameUsage[nameNum]);
      dirNameUsage[nameNum]++;
      //currD->length = sizeof(currD->name);
      currD->next = NULL;
    }
    int nDirs = rand() % (maxNumDirs - minNumDirs + 1) + minNumDirs;
    int nFiles = rand() % (maxNumFiles - minNumFiles + 1) + minNumDirs;
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
    defaultBuildTree(currD, newDepth, nDirs, nFiles, depthFromRoot);
  }
  return;
}

int max(int num1, int num2) {
  int result;
 
  if (num1 > num2)
    result = num1;
  else
    result = num2;
 
  return result; 
}

int TreeDepth( dir *root ){
  if(root->dirs == NULL){
    return 0;
  }

  dir * curr = root->dirs;
  int maxDepth = 1;
  while(curr != NULL){
    maxDepth = max(maxDepth, TreeDepth(curr));
    //printf("depth = %d\n", maxDepth);
    curr = curr->next;
  }
  return maxDepth +1;
}



dir * lookupParent( dir * root, char * path ){
  //expect path in format of root/../
  char c;
  char * name;
  int len = strlen(path);
  dir * parentDir = NULL;
  int lastSlash = -1;
  int depth = 0;
  int i;

  assert (path[0] != '/');
  assert (path[len-1] == '/');


  for (i=0; i< len; i++){
    if (path[i] == '/'){
      depth++;
      
      //the current name goes from path[lastSlash+1] to path[i-1]

      if (depth == 1){
	//if depth =1 assert this is root and we don't need to search for it
	assert(lastSlash = -1);
	//assert (root->length == (i));
	//assert(0 == strncmp(root->name, path, root->length));
	parentDir = root;
	lastSlash = i;
      } 
      if (i==len-1){
	//this is the last thing in the path 
	//should I still search and verify we find it? 
	if (depth ==1){
	  //it is also the root
	  //if we passed in "root/" what should we return? NULL?
	  return NULL;
	}
	assert (parentDir != NULL);
	return parentDir;
      } else if (depth != 1){
	dir * iterDir;
	bool found = false;
	int dirNameLen;

	//this is not the root and it is not last thing in the path
	iterDir = parentDir->dirs;
	while (!found && (iterDir != NULL)){
	  dirNameLen = (i-1) - (lastSlash+1) +1;
	  if (0 == strncmp(iterDir->name, &(path[lastSlash+1]), dirNameLen)){
	    found = true;
	    parentDir = iterDir;
	  } else {
	    iterDir = iterDir->next;
	  }
	  
	}
	assert(found);
	lastSlash = i;
	depth++;

      }
    }

  }

  assert(0);
  assert(parentDir != NULL);
  return parentDir;
}

void workloadGen(dir *root, char * path, char Depth, int sLocal, bool strictSLocal,
		 int tLocal, int numEntries, FILE * out)
{
  //How deep is the tree
  int maxTreeDepth = TreeDepth(root);
  int minDepth;
  int maxDepth;

  if(Depth == 'A'){
    minDepth = 1;
    maxDepth = maxTreeDepth;
  }else if( Depth == 'S' ){
    minDepth = 1;
    if(maxTreeDepth % 3 == 0){
      maxDepth = maxTreeDepth/3;
    }else{
      maxDepth = maxTreeDepth/3 + 1;
    }
  }else if( Depth == 'M' ){
    if(maxTreeDepth % 3 == 0){
      minDepth = maxTreeDepth/3 + 1;
      maxDepth = 2 * maxTreeDepth/3;
    }else if(maxTreeDepth % 3 == 1){
      minDepth = maxTreeDepth/3 + 2;
      maxDepth = 2 * maxTreeDepth/3 + 1;
    }else{
      minDepth = maxTreeDepth/3 + 2;
      maxDepth = 2 * maxTreeDepth/3 + 2;
    }
  }else if( Depth == 'D' ){
    if(maxTreeDepth % 3 == 0){
      minDepth = 2 * maxTreeDepth/3 + 1;
      maxDepth = 3 * maxTreeDepth/3;
    }else if(maxTreeDepth % 3 == 1){
      minDepth = 2 * maxTreeDepth/3 + 2;
      maxDepth = 3 * maxTreeDepth/3 + 1;
    }else{
      minDepth = 2 * maxTreeDepth/3 + 3;
      maxDepth = 3 * maxTreeDepth/3 +2;
    }  
  }else{
    //This is for Random Min Max Depth
    while(1){
      int r1 = rand() % maxTreeDepth + 1;
      int r2 = rand() % maxTreeDepth + 1;

      if(r1 != r2){
	if( r1 > r2 ){
	  maxDepth = r1;
	  minDepth = r2;
	}else{
	  minDepth = r1;
	  maxDepth = r2;
	}
	break;
      }
    }
  }

  //Pick Starting Location Depth
  int depth = rand() % (maxDepth - minDepth + 1) + (minDepth);
  //Pick Starting Location ... curr ... the parent directory
  int whichAtDepth = rand() % depthInfoArray[depth].num;
  dir * curr = depthInfoArray[depth].first;

  for( int i = 0; i< whichAtDepth; i++ ){
    curr = curr->depthNext;
  }

  
  //Walk path back to root to get build path
  
  char * currPath = (char *)malloc(sizeof(char) * 1024);
  char * tempPath = (char *)malloc(sizeof(char) * 1024);
  char * lastPrint = (char *)malloc(sizeof(char) * 1024);
  assert(currPath);
  assert(tempPath);
  assert(lastPrint);

  sprintf(tempPath, "%s/", curr->name);
  dir * parent = curr->parent;
  while(parent != NULL){
    sprintf(currPath, "%s/%s", parent->name, tempPath);
    parent = parent->parent;
  }
  //pick a file or directory in curr
  //print path with dir or file name tacked on.
  if(rand() % 2 == 0 || curr->dirs == NULL){
    //pick new file                                                                                              
    int numFiles = 0;
    file * currF = curr->files;
    while( currF != NULL ){
      numFiles++;
      currF = currF->next;
    }
    currF = curr->files;
    
    int	whichFile = rand() % numFiles;
    for(int i = 1; i <= whichFile; i++){
      currF = currF->next;
    }
    sprintf( lastPrint, "%s%s\n", currPath, currF->name);
    fprintf( out, "%s", lastPrint);
    
  }else{
    //pick new dir                                                                                               
    int numDirs = 0;
    dir * currD = curr->dirs;
    while( currD != NULL ){
      numDirs++;
      currD = currD->next;
    }
    currD = curr->dirs;
    
    int whichDir = rand() % numDirs;
    for(int i = 1; i <= whichDir; i++){
      currD = currD->next;
    }
    sprintf( lastPrint,"%s%s/\n", currPath, currD->name);
    fprintf( out, "%s", lastPrint);
  }


   
 
  //Loop once for each entry needed
  for(int i = 0; i < numEntries-1 ; i++){
    if(rand() % 100 <= tLocal){
      //reprint lastPrint
      fprintf( out, "%s", lastPrint);
      continue;
    }
    if(strictSLocal){
      //counter for weird cases
      int limit = 10;
      int counter = 0;
      while(counter < limit){
	int stay = rand() % 100;
	if(stay <= sLocal){
	  if(rand() % 2 == 0 || curr->dirs == NULL){
	    //pick new file
	    int numFiles = 0;
	    file * currF = curr->files;
	    while( currF != NULL ){
	      numFiles++;
	      currF = currF->next;
	    }
	    currF = curr->files;

	    int whichFile = rand() % numFiles;
	    for(int i = 1; i <= whichFile; i++){
	      currF = currF->next;
	    }
	    sprintf( lastPrint, "%s%s\n", currPath, currF->name);
	    fprintf( out, "%s", lastPrint);
	    break;
	  }else{
	    //pick new dir
	    int numDirs = 0;
            dir * currD = curr->dirs;
            while( currD != NULL ){
	      numDirs++;
	      currD = currD->next;
            }
            currD = curr->dirs;

            int	whichDir = rand() % numDirs;
            for(int i = 1; i <= whichDir; i++){
	      currD = currD->next;
            }
	    sprintf( lastPrint, "%s%s/\n", currPath, currD->name);
	    fprintf( out, "%s", lastPrint);
            break;
	  }

	}else{
	  int upORdown;

	  if((curr->dirs == NULL && depth != minDepth) || depth == maxDepth){
	    upORdown = 0;
	    counter++;
	  }else if(depth == minDepth && curr->dirs != NULL){
	    upORdown = 1;
	    counter++;
	  }else if((depth != minDepth || depth != maxDepth) && curr->dirs != NULL){
	    //flip coin
	    if(rand() % 2 == 0){
	      upORdown = 0;
	    }else{
	      upORdown = 1;
	    }
	    counter++;
	  }else{
	    printf("Subtree chosen is too shallow and has no children\n");
	    exit(-1);
	  }

	  if( upORdown== 0){
	    //move dir up
	    curr = curr->parent;
	    //Track Path
	    int len = strlen(currPath);
	    int start = len - 2;
	    char c = currPath[start];
	    while( c != '/' ){
	      start = start - 1;
	      c = currPath[start];
	    }
	    currPath[start+1] = '\0';
	    depth = depth - 1;
	  }else{
	    //move dir down
            //pick new child dir move down
	    int numDirs = 0;
            dir * currD = curr->dirs;
            while( currD != NULL ){
              numDirs++;
              currD = currD->next;
            }
            currD = curr->dirs;

            int whichDir = rand() % numDirs;
            for(int i = 1; i <= whichDir; i++){
              currD = currD->next;
            }

            curr = currD;
	    sprintf(currPath, "%s%s/", currPath, curr->name);
            depth++;
	  }
	}
      }
      //print new location
      continue;
    }else{
      int moveChance = 5;
      if(rand()% 100 <= moveChance){
	//major move across tree;
	//Pick Starting Location Depth       
	int depth = rand() % (maxDepth - minDepth + 1) + (minDepth);
	//Pick Starting Location ... curr ... the parent directory
	int whichAtDepth = rand() % depthInfoArray[depth].num;
	dir * curr = depthInfoArray[depth].first;

	for( int i = 0; i< whichAtDepth; i++ ){
	  curr = curr->depthNext;
	}

	//Walk path back to root to get build path
	sprintf(tempPath, "%s/", curr->name);
	parent = curr->parent;
	while(parent != NULL){
	  sprintf(currPath, "%s/%s", parent->name, tempPath);
	  parent = parent->parent;
	}
	fprintf(out, "%s\n", currPath);
	sprintf(lastPrint, "%s", currPath);
	continue;
      }
      
      //counter for weird cases
      int limit = 10;
      int counter = 0;
      while(counter < limit){
        int stay = rand() % 100;
        if(stay <= sLocal){
          if(rand() % 2 == 0 || curr->dirs == NULL){
            //pick new file                                                                                              
            int numFiles = 0;
            file * currF = curr->files;
            while( currF != NULL ){
	      numFiles++;
	      currF = currF->next;
            }
            currF = curr->files;

            int	whichFile = rand() % numFiles;
            for(int i = 1; i <= whichFile; i++){
	      currF = currF->next;
            }
	    sprintf( lastPrint, "%s%s\n", currPath, currF->name);
	    fprintf( out, "%s", lastPrint);
            break;
          }else{
            //pick new dir                                                                                               
            int numDirs = 0;
            dir * currD = curr->dirs;
            while( currD != NULL ){
              numDirs++;
              currD = currD->next;
            }
            currD = curr->dirs;

            int whichDir = rand() % numDirs;
            for(int i = 1; i <= whichDir; i++){
              currD = currD->next;
            }
	    sprintf( lastPrint, "%s%s/\n", currPath, currD->name);
	    fprintf( out, "%s", lastPrint);
            break;
          }


        }else{
          int upORdown;
          //need to define min and max Depth and track active depth
          if((curr->dirs == NULL && depth != minDepth) || depth == maxDepth){
            upORdown = 0;
	    counter++;
          }else if(depth == minDepth && curr->dirs != NULL){
            upORdown = 1;
	    counter++;
          }else if((depth != minDepth || depth != maxDepth) && curr->dirs != NULL){
            //flip coin
            if(rand() % 2 == 0){
	      upORdown = 0;
            }else{
	      upORdown = 1;
            }
            counter++;
          }else{
            //printf("Subtree chosen is too shallow and has no children\n");
	    int depth = rand() % (maxDepth - minDepth + 1) + (minDepth);
	    int whichAtDepth = rand() % depthInfoArray[depth].num;
	    dir * curr = depthInfoArray[depth].first;

	    for( int i = 0; i< whichAtDepth; i++ ){
	      curr = curr->depthNext;
	    }

	    sprintf(tempPath, "%s/", curr->name);
	    parent = curr->parent;
	    while(parent != NULL){
	      sprintf(currPath, "%s/%s", parent->name, tempPath);
	      parent = parent->parent;
	    }
	    fprintf(out, "%s\n", currPath);
	    sprintf(lastPrint, "%s", currPath);
	    upORdown = -1;
	    break; //out of while
          }

          if( upORdown== 0){
            //move dir up
	    curr = curr->parent;

	    int len = strlen(currPath);
            int start =len - 2;
            char c = currPath[start];
            while( c !='/' ){
	      start = start - 1;
	      c = currPath[start];
            }
            currPath[start+1] ='\0';

            depth = depth - 1;
          }else if(upORdown = 1){
            //move dir down
            //pick new child dir move down
	    int numDirs = 0;
            dir * currD = curr->dirs;
            while( currD != NULL ){
              numDirs++;
              currD = currD->next;
            }
            currD = curr->dirs;

            int whichDir = rand() % numDirs;
            for(int i = 1; i <= whichDir; i++){
              currD = currD->next;
            }

	    curr = currD;
	    sprintf(currPath, "%s%s/", currPath, curr->name);
	    depth++;
          }
        }
      }
      continue;
    }
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
  root.parent = NULL;
  //root.length = 4;

  int nextArg = 1;
  bool isUniform = false;
  int minDepth;
  int maxDepth;
  int minWidth;
  int maxWidth;
  int minName;
  int maxName;
  int chosenDepth;
  int numDirs;
  int numFiles;
  bool isNewBuild = false;
  bool isDefault = false;
  
  FILE *in;
  int parentNameLength;


  int tLocal;
  int numEntries;
  int sLocal;
  bool isStrict = false;
  char wDepth;
  FILE * workOut;
  bool doWorkload = false;

  if(argc == 1){
    numDirs = rand() % (maxNumDirs - minNumDirs + 1) + minNumDirs;
    numFiles = rand() % (maxNumFiles - minNumFiles + 1) + minNumFiles;
    //int depth = maxTreeDepth;
    chosenDepth = rand() % (maxTreeDepth - minTreeDepth + 1) + minTreeDepth;
    isDefault = true;
    //defaultBuildTree(&root, depth, numDirs, numFiles);
  }

  //int nextArg = 1;
  else{

    if(strcmp(argv[nextArg], "-B") == 0){
      isNewBuild = true;
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
	chosenDepth = minDepth;
      }else{
	chosenDepth = rand() % (maxDepth - minDepth + 1) + minDepth;
	printf("chosenDepth = %d\n", chosenDepth);
      }

      numDirs = rand() % (maxWidth - minWidth + 1) + minWidth;
      numFiles = rand() % (maxWidth - minWidth + 1) + minWidth;
      /*if(isUniform){
	uniformBuildTree(&root, depth, numDirs, numFiles, minName, maxName);
      }else{
	buildTree(&root, depth, numDirs, numFiles, minWidth, maxWidth, minName, maxName);
	}*/
    }

    else if(strcmp(argv[nextArg], "-F") == 0){
      if(argc < 3){
	printf("Not enough argument for building a tree from a file\n");
	return -1;
      }
      //char * filename = argv[nextArg + 1];
      printf( "%s\n", argv[nextArg + 1]);
      //FILE * in;
      in = fopen(argv[nextArg + 1], "rw");
      if( in == NULL ){
	printf("Could not open file containing filesystem tree\n");
	return -1;
      }

      //grab D or F
      char c = fgetc(in);
      //keep reading until a slash
      while(c != '/'){
	c = fgetc(in);
      }
      root.name = (char *)malloc(sizeof(char)*25);
      //keep track of name length?
      //build name
      c = fgetc(in);
      while(c != '/'){
	strncat(root.name, &c, 1);
	c = fgetc(in);
      }

      c = fgetc(in);
      if(c == '\n'){
	printf("Success in grabing newline\n");
      }
      //int numSlashes = 2;
      parentNameLength = 8;
      //fileBuildTree(&root, in, parentNameLength, "/root/");

      nextArg = 3;
    }

    else if(strcmp(argv[nextArg], "-W") == 0){
      numDirs = rand() % (maxNumDirs - minNumDirs + 1);
      numFiles = rand() % (maxNumFiles - minNumFiles + 1);
      //int depth = maxTreeDepth;                                                                                                          
      chosenDepth = rand() % (maxTreeDepth - minTreeDepth + 1);
      isDefault = true;
      //defaultBuildTree(&root, depth, numDirs, numFiles);
    }
    
    else{
      printf("INVALID first argument\n");
      return -1;
    }


    //Deal with Workload parsing
    if(argc > nextArg){
      if(strcmp(argv[nextArg], "-W") == 0){
	
	doWorkload = true;
	
	if(argc < nextArg + 12){
	  printf("Not enough arguments for generating workload\n");
	  return -1;
	}

	for(int i = nextArg+1; i< (nextArg + 12); i = i+2){
	  if(strcmp(argv[i], "Depth=") == 0){
	    if(strcmp(argv[i+1], "S") == 0){
	      wDepth = argv[i+1][0];
	    }else if(strcmp(argv[i+1], "M") == 0){
	      wDepth = argv[i+1][0];
	    }else if(strcmp(argv[i+1], "D") == 0){
	      wDepth = argv[i+1][0];
	    }else if(strcmp(argv[i+1], "R") == 0){
	      wDepth = argv[i+1][0];
	    }else if(strcmp(argv[i+1], "A") == 0){
	      wDepth = argv[i+1][0];
	    }else{
	      printf("Invalid Depth option\n");
	      return -1;
	    }
	  }else if(strcmp(argv[i], "sLocal=") == 0){
	    if(strcmp(argv[i+1], "R") == 0){
              sLocal = rand() % 100 + 1;
            }else{
	      if(atoi(argv[i+1])<=100 && atoi(argv[i+1])>0){
		sLocal = atoi(argv[i+1]);
	      }
	      else{
		printf("sLocal not 1-100");
		return -1;
	      }
            }
	  }else if(strcmp(argv[i], "sRestrict=") == 0){
	    if(strcmp(argv[i+1], "S") == 0){
	      isStrict = true;
	    }else if(strcmp(argv[i+1], "N") == 0){
	      isStrict = false;
	    }else{
	      printf("Invalid Spatial Restriction Option\n");
	      return -1;
	    }
	  }else if(strcmp(argv[i], "tLocal=") == 0){
	    if(strcmp(argv[i+1], "R") == 0){
	      tLocal = rand() %100;
            }else{
	      if(atoi(argv[i+1])<=100 && atoi(argv[i+1])>0){
                tLocal = atoi(argv[i+1]);
	      } 
	      else{
		printf("tLocal not 1-100");
		return -1;
              }
            }
	  }else if(strcmp(argv[i], "Num=") == 0){
	    if(strcmp(argv[i+1], "R") == 0){
	      numEntries = rand()%1000;
	    }else{
	      numEntries = atoi(argv[i+1]);
	    }
	    
	  }else if(strcmp(argv[i], "File=") == 0){
	    workOut = fopen(argv[i+1], "w");
	    if(workOut == NULL){
	      printf("Failed to open output file for workload\n");
	      return -1;
	    }
	  }else{
	    printf( "Unknown work argument format\n");
	    return -1;
	  }
	} //end of for loop

	//workloadGen(&root, "root/", depth, sLocal, isStrict, tLocal, numEntries, workOut); 

      }  //end of if -w
    } //End of if more args available
  } //End of Else for more than 1 arg  



  //malloc the depth array
  depthInfoArray = (struct depthInfo *) malloc( maxTreeDepth * sizeof(depthInfo));
  assert(depthInfoArray);
  for (int i=0; i< maxTreeDepth; i++){
    depthInfoArray[i].first = NULL;
    depthInfoArray[i].num = 0;
  }


  maxChosenDepth = chosenDepth;
  
  time_t t;
  srand((unsigned) time(&t));
  
  //CALL BUILD Tree functions
  if( isNewBuild == true ){
    if(isUniform){
      uniformBuildTree(&root, chosenDepth, numDirs, numFiles, minName, maxName);
    }else{
      buildTree(&root, chosenDepth, numDirs, numFiles, minWidth, maxWidth, minName, maxName,0);
    }
  }else if( isDefault == true){
    defaultBuildTree(&root, chosenDepth, numDirs, numFiles, 0);
  }else{
    root.whichDepth = 0;
    fileBuildTree(&root, in, parentNameLength, "/root/", 0);
  }


  //printf("Depth =  %d\n", TreeDepth(&root));
  //return 0;
  

  //CAll workload Gen                                                                                                                  
  if ( doWorkload == true ){
    workloadGen(&root, "root/", chosenDepth, sLocal, isStrict, tLocal, numEntries, workOut);
  }


  for( int i = 0; i < maxTreeDepth; i++ ){
    printf("DEPTH %d\n\n", i);
    dir * curr = depthInfoArray[i].first;
    for( int j = 0; j < depthInfoArray[i].num ; j++ ){
      printf("%s ", curr->name);
      curr = curr->depthNext;
    }
    printf("\n\n");
  }
  



  //lookup parent test
#define LOOKUP_PARENT_TEST 0
#if LOOKUP_PARENT_TEST  
  char * testerName = "log0";
  dir * tester = lookupParent(&root, "root/log0/ewok0/"); 

  assert
  if (0==strncmp(tester->name, testerName, strlen(testerName))){
    printf("Success\n");
  }else{
    printf("FAILURE\n");
  }
  
#endif

  if( isDefault == true || isNewBuild == true ){
    FILE * out;
    out = fopen("filesystem.txt", "w");
    if( out == NULL) {
      printf("Can't open file to output new file system to\n");
      return -1;
    }
    char * path = "/";
    printTree(&root, path, out);
  }


  return 0;
  
}

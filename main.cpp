#include<stdio.h>
#include<stdlib.h>
#include<dirent.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<iostream>
#include<bits/stdc++.h>
#include<termios.h>
#include<fcntl.h>
#include<pwd.h>
#include<grp.h>
#include<sys/ioctl.h>
#include<time.h>
#define EXEC_PATH "/usr/bin/xdg-open"
#define EXEC_NAME "xdg-open"

using namespace std;

/*----------------------------------------------

GLOBAL VARIABLES

-----------------------------------------------*/

bool commandMode = false;
struct termios new_settings;
struct termios original_settings;
struct termios command_settings;
int wincol;
int winrow;
int cursorx = 1;
int cursory = 1;
int xoffset = 0;
int yoffset = 0;
int bottomlimit = 2;
int quitflag = 1;

vector<string> outputBuffer;
string commandBuffer = "";
string currPath; 
string homePath;
vector<string> dirlist;
stack<string> prevdirs;
stack<string> nextdirs;
string logstr;

/*----------------------------------------------

ERROR HANDLING

-----------------------------------------------*/

//Kill application on error with message
void die(string s){
	perror(s.c_str());
	exit(1);
}

/*----------------------------------------------

TERMINAL HANDLING

-----------------------------------------------*/

//Switch to normal mode input
void setNormalMode(){
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_settings) == -1) die("Could not switch to normal mode");
	commandMode = false;
	cout << "\x1b[?25l";
	cursory = 1;
	cursorx = 1;
}

//Switch to command mode input
void setCommandMode(){
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &command_settings) == -1) die("Could not switch to command mode");
	logstr="";
	commandMode = true;
	cout << "\x1b[?25h";
	cout << "\033[2J\033[1;1H";
}

void setOriginalMode(){
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_settings) == -1) die("Could not restore original terminal: restart");
	cout << "\x1b[?25h";
	cout << "\033[2J\033[1;1H";
}


//Prepare termios structs
void prepareTerminalInfo(){
	if(tcgetattr(STDIN_FILENO, &original_settings) == -1) die("Could not get terminal info");
	new_settings = original_settings;
	command_settings = original_settings;
	
	new_settings.c_lflag &= ~(ICANON | ECHO);
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	
	command_settings.c_lflag &= ~(ICANON);
	command_settings.c_cc[VMIN] = 1;
	command_settings.c_cc[VTIME] = 0;
	
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_settings) == -1) die("Could not open file explorer");
	
	atexit(setOriginalMode);
}

//Get current window size
int updateWindowSize() {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
		return -1;
	} 
	else{
		wincol = ws.ws_col;
		winrow = ws.ws_row;
		return 0;
	}
}

//Reprint outputbuffer on screen
void refreshScreen(){
	string line;
	if(winrow >= bottomlimit+1){
		if(!commandMode){
			cout << "\033[2J\033[1;1H";
			for(int i = 0; i < winrow-bottomlimit; i++){
				if(yoffset+i < outputBuffer.size()){
					line = outputBuffer[yoffset+i];
					if(xoffset < line.size()) cout << line.substr(xoffset, wincol) << endl;
					else cout << endl; 
				}
				else{
					cout << endl;
				}
			}
			
			line = "\n----Normal mode: " + currPath;
			cout << line.substr(0, wincol);
			cout << "\033[" << cursory << ";" << cursorx << "H";
			cout << (unsigned char)95 << endl;
		}
		else{
			cout << "\033[2J\033[1;1H";
			for(int i = 0; i < winrow-bottomlimit; i++){
				if(yoffset+i < outputBuffer.size()){
					line = outputBuffer[yoffset+i];
					if(xoffset < line.size()) cout << line.substr(xoffset, wincol) << endl;
					else cout << endl; 
				}
				else{
					cout << endl;
				}
			}
			line = logstr;
			cout << "----Command mode: " << line.substr(0, wincol-19) << endl;
		}
	}
	else{
		cout << "\033[2J\033[1;1H";
		cout << "Please increase screen size" << endl;
		cursorx = 1;
		cursory = 1;
		yoffset = 0;
		xoffset = 0;
	}
}

/*----------------------------------------------

GLOBAL DIRECTORY MANAGEMENT

-----------------------------------------------*/

//Get current working directory and store to currPath variable as string
void getcwd(){
	char* currPath_c = new char[PATH_MAX+1];
	getcwd(currPath_c, PATH_MAX+1);
	currPath = string(currPath_c);
}

void setHomeDir(){
	struct passwd* userdetails = getpwuid(getuid());  // Check for NULL!
	homePath = "/home/" + string(userdetails->pw_name);
}

/*----------------------------------------------

OUTPUT BUFFER CREATION

-----------------------------------------------*/

//Receives some directory vector and stores all its contents to output buffer
void printScreenC(vector<string>dirlist){
	outputBuffer.clear();
	int i = 1;
	for(int row = 0; row < dirlist.size(); row++){
		struct stat dirDetails;
		char* permissions = new char[9];
		string path;
		if(lstat((currPath+"/"+dirlist[row]).c_str(), &dirDetails) != -1){
			string temp = "----------";
			strcpy(permissions, temp.c_str());
			
			intmax_t mode = (intmax_t) dirDetails.st_mode;
			if ( S_ISDIR(mode) ) permissions[0] = 'd';
			if ( mode & S_IRUSR ) permissions[1] = 'r';    /* 3 bits for user  */
			if ( mode & S_IWUSR ) permissions[2] = 'w';
			if ( mode & S_IXUSR ) permissions[3] = 'x';

			if ( mode & S_IRGRP ) permissions[4] = 'r';    /* 3 bits for group */
			if ( mode & S_IWGRP ) permissions[5] = 'w';
			if ( mode & S_IXGRP ) permissions[6] = 'x';

			if ( mode & S_IROTH ) permissions[7] = 'r';    /* 3 bits for other */
			if ( mode & S_IWOTH ) permissions[8] = 'w';
			if ( mode & S_IXOTH ) permissions[9] = 'x';
			
			intmax_t timevar = (intmax_t) dirDetails.st_size;
			vector<string> suff = {"B", "KB", "MB", "GB", "TB"};
			
			
			
			int i = 0;
			while(i < 4){
				if(timevar/1024 > 0){
					timevar = timevar/1024;
					i++;
				}
				else{
					break;
				}
			}
			
			string timestr = to_string(timevar) + suff[i];
			
			struct passwd *pw = getpwuid(dirDetails.st_uid);
			struct group *gr = getgrgid(dirDetails.st_gid);
			char* modtime = ctime(&dirDetails.st_mtime);
			modtime[strlen(modtime)-1] = '\0';
			
			path = dirlist[row];
			
			outputBuffer.push_back(to_string(row) + string("   ") + timestr + string("   ") + string(permissions) + string("   ") + string(pw->pw_name) + string("   ") + string(gr->gr_name) + string("   ") + string(modtime) + string("   ") + path);
		}
		
		else{	
			outputBuffer.push_back(to_string(row) + string("   ") + string("     ") + string("   ") + string("---------") + string("   ") + string("   ") + string("   ") + string("   ") + string("   ") + string("   ") + string("   ")  + path);  
		}
	}
}

/*----------------------------------------------

DIRECTORY NAVIGATION

-----------------------------------------------*/

//Goes to directory given path and creates new dirlist vector for it
void gotodir(string path, int abs_flag = 1){
	if(abs_flag==1){
		stack<std::string>().swap(nextdirs);
		prevdirs.push(currPath);
	}
	
	if(chdir(path.c_str())!=0){
		logstr = "No such directory";
	};
	
	int count;
	count = 0;
	getcwd();
	DIR* stream = opendir(currPath.c_str());
	struct dirent* dir;
	
	dirlist.clear();
	
	if(stream){
		while((dir = readdir(stream)) != NULL){
			dirlist.push_back(string(dir->d_name));
		}
		closedir(stream);
	}
	else{
		logstr = "No such directory";
		exit(1);
	}
	
	sort(dirlist.begin(), dirlist.end());
	printScreenC(dirlist);
}

//Goes to directory given a number input (made for normal mode)
void gotodirN(int n){
	if(n < dirlist.size()) gotodir(dirlist[n]);
}

void goprevdir(){
	if(!prevdirs.empty()){
		nextdirs.push(currPath);
		gotodir(prevdirs.top(), 0);
		prevdirs.pop();
	}
}

void gonextdir(){
	if(!nextdirs.empty()){
		prevdirs.push(currPath);
		gotodir(nextdirs.top(), 0);
		nextdirs.pop();
	}
}

/*----------------------------------------------

COPY

-----------------------------------------------*/

//Helper function: copy
void copyFile(string sourcepath, string destpath, struct stat sourceDetails){
	char* buffer = new char[sourceDetails.st_size];
	int source_fd = open(sourcepath.c_str(), O_RDONLY);
	int dest_fd = open(destpath.c_str(), O_WRONLY | O_CREAT, sourceDetails.st_mode);
	int readret;
	int writeret;
	while(readret = read(source_fd, buffer, sourceDetails.st_size)){
		if (readret == -1){
			logstr = "Cannot read: " + sourcepath;
			return;
		}
		writeret = write(dest_fd, buffer, sourceDetails.st_size);
		if (writeret == -1){
			logstr = "Could not write to " + destpath;
			return;
		}
	}
	
	fchown(dest_fd, sourceDetails.st_uid, sourceDetails.st_gid);
	
	close(source_fd);
	close(dest_fd);
}

//Helper function: copy
void copyDir(string sourcepath, string destpath, struct stat sourceDetails){
	mkdir(destpath.c_str(), sourceDetails.st_mode);

	DIR* tempstream = opendir(sourcepath.c_str());
	vector<string> tempdirlist;
	struct dirent* dir;
	if(tempstream){
		while(dir = readdir(tempstream)){
			tempdirlist.push_back(string(dir->d_name));
		}
		closedir(tempstream);
	}
	else{
		logstr = "Error while copying: " + sourcepath;
		return;
	}
	
	sort(tempdirlist.begin(), tempdirlist.end());
	
	struct stat tempdetails;
	string tempsourcepath;
	string tempdestpath;
	
	for(int i = 2; i < tempdirlist.size(); i++){
		tempsourcepath = sourcepath + "/" + tempdirlist[i];
		tempdestpath = destpath + "/" + tempdirlist[i];
		if(lstat(tempsourcepath.c_str(), &tempdetails) == -1){
			logstr = "Copy for some dir(s) failed";
			continue;
		}
		
		if(S_ISREG(tempdetails.st_mode)){
			copyFile(tempsourcepath, tempdestpath, tempdetails);
		}
		else if(S_ISDIR(tempdetails.st_mode)){
			copyDir(tempsourcepath, tempdestpath, tempdetails);
		}
	}
}

//Copies file from source to destination
void copy(string sourcepath, string destpath){

	int index = sourcepath.size()-1;
	int len = 0;

	struct stat destDetails;
	if((lstat(destpath.c_str(), &destDetails)) == -1){
		logstr = "Destination does not exist";
		return;
	}
	
	while(index > 0 && sourcepath[index] != '/' && sourcepath[index] != '\\'){
		index--;
		len++;
	}
	
	string destination;
	
	if(index > 0) destination = destpath + "/" + sourcepath.substr(index+1, len+1);
	else destination = destpath + "/" + sourcepath;
	
	struct stat sourceDetails;
	if((lstat(sourcepath.c_str(), &sourceDetails)) != -1){
		
	}
	else{
		logstr = "Given source could not be accessed";
		return;
	}
	
	if(S_ISREG(sourceDetails.st_mode)){
		copyFile(sourcepath, destination, sourceDetails);
	}
	else if(S_ISDIR(sourceDetails.st_mode)){
		copyDir(sourcepath, destination, sourceDetails);
	}
	
	gotodir(".", 0);
}

/*----------------------------------------------

RENAME

-----------------------------------------------*/

//Renames file/folder in current directory
void myrename(string oldfname, string newfname){
	for(char c: oldfname) if (c=='/' || c=='\\') {
		logstr = "Invalid sourcename";
		return;
	}
	for(char c: newfname) if (c=='/' || c=='\\') {
		logstr = "Invalid newname";
		return;
	}
	
	if(rename(oldfname.c_str(), newfname.c_str()) != 0){
		logstr = "Rename unsuccessful";
	};
	
	gotodir(".", 0);
}

/*----------------------------------------------

MOVE

-----------------------------------------------*/

//Moves file/folder from source/file1 to dest/file2
void move(string sourcepath, string destpath){
	int index = sourcepath.size()-1;
	int len = 0;
	
	struct stat destDetails;
	if((lstat(destpath.c_str(), &destDetails)) == -1){
		logstr = "Destination does not exist";
		return;
	}
	
	while(index > 0 && sourcepath[index] != '/' && sourcepath[index] != '\\'){
		index--;
		len++;
	}
	
	string destination;
	
	if(index > 0) destination = destpath + "/" + sourcepath.substr(index+1, len+1);
	else destination = destpath + "/" + sourcepath;
	
	
	if(rename(sourcepath.c_str(), destination.c_str()) != 0){
		//logstr = "Move some file(s) unsuccessful";
		logstr = "FAILURE:: Source: "+ sourcepath + " Destination: " + destination;
	};
	
	gotodir(".", 0);
}

/*----------------------------------------------

FILE/DIRECTORY CREATION

-----------------------------------------------*/

//Creates new file with default rights 664
void create_file(string file, string path){
	path = path + "/" + file;
	
	int fd = open(path.c_str(), O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if(fd == -1){
		logstr = "Could not create file";
	}
	close(fd);
	
	gotodir(".", 0);
}

void create_dir(string folder, string path){
	path = path + "/" + folder;
	if(mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0){
		logstr = "Could not create directory";
	}
	
	gotodir(".", 0);
}

/*----------------------------------------------

DELETE

-----------------------------------------------*/

//Helper function: delete
void delfile(string path){
	//path = evalDir(path);
	//cout << path << endl;
	if(unlink(path.c_str()) != 0){
		logstr = "Could not delete " + path; 
	}

}

//Helper function: delete
void deldir(string path){
	DIR* tempstream = opendir(path.c_str());
	vector<string> tempdirlist;
	struct dirent* dir;
	if(tempstream){
		while(dir = readdir(tempstream)){
			tempdirlist.push_back(string(dir->d_name));
		}
		closedir(tempstream);
	}
	else return;
	
	sort(tempdirlist.begin(), tempdirlist.end());
	
	struct stat tempdetails;
	string temppath;
	
	for(int i = 2; i < tempdirlist.size(); i++){
		temppath = path + "/" + tempdirlist[i];
		if(lstat(temppath.c_str(), &tempdetails) == -1){
			logstr = "Could not delete: " + temppath;
			return;
		}
		
		if(S_ISREG(tempdetails.st_mode)){
			delfile(temppath);
		}
		else if(S_ISDIR(tempdetails.st_mode)){
			deldir(temppath);
		}
	}
	
	rmdir(path.c_str());

}

//Recursively deletes folder and its contents, or deletes file only
void del(string path){
	struct stat pathDetails;
	if((lstat(path.c_str(), &pathDetails)) == -1){
		logstr = "Invalid path";
		return;
	}
	
	if(S_ISREG(pathDetails.st_mode)){
		delfile(path);
	}
	else if(S_ISDIR(pathDetails.st_mode)){
		deldir(path);
	}
	
	gotodir(".", 0);
}

/*----------------------------------------------

SEARCH

-----------------------------------------------*/

//Recursively looks for given file in all subdirectories of currPath (given as parameter from kickstarter search)
bool searchHelper(string path, string file){
	DIR* stream = opendir(path.c_str());
	struct dirent* dir;
	vector<string> dirlisttemp;
	struct stat itemstat;
	if(stream){
		while(dir = readdir(stream)){
			dirlisttemp.push_back(string(dir->d_name));
		}
		closedir(stream);
	}
	else{
		return false;
	}
	
	
	for(string item : dirlisttemp){
		if(item != "." && item != ".."){
			if(lstat((path + "/" + item).c_str(), &itemstat) == -1){
				continue;
			}
			
			if(S_ISREG(itemstat.st_mode)){
				if (item == file) return true;
			}
			else if(S_ISDIR(itemstat.st_mode)){
				if(searchHelper(path + "/" + item, file)){
					return true;
				}
			}
		}
	}
	
	return false;
}

//Kickstarter for search function, made separate for readability
bool search(string file){
	if(searchHelper(currPath, file)){
		logstr = "True";
		return true;
	}
	else{
		logstr = "False";
		return false;
	}
}

/*----------------------------------------------

INPUT PROCESSING

-----------------------------------------------*/

//Empty space for a command parsing function
vector<string> parseCommand(string commandphrase){
	
	int subflag = 1;
	for(int i = 0; i < commandphrase.size(); i++){
		if(commandphrase[i] == '\''){
			subflag *= -1;
		} 
		if(commandphrase[i] == ' ' && subflag == -1){
			commandphrase[i] = '?';
		}
	}
	
	vector<string> command;
	
	int start = 0;
	for(int i = 0; i < commandphrase.size(); i++){
		if(commandphrase[i] == ' '){
			command.push_back(commandphrase.substr(start, i-start));
			start = i+1;
		}
		else if(i == commandphrase.size()-1){
			command.push_back(commandphrase.substr(start, commandphrase.size()-start));
		}
		
	}
	
	int len;
	for(int i = 0; i < command.size(); i++){
		len = command[i].size();
		if (command[i][0] == command[i][len-1] && command[i][0] == '\''){
			command[i] = command[i].substr(1, len-2);
			for(int j = 0; j < command[i].size(); j++){
				if (command[i][j] == '?') command[i][j] = ' ';
			}
		}
	}
	
	return command;
}

//Process command typed in command mode
void processCommand(string commandphrase){
	if (commandphrase == "") return;
	
	logstr = "";
	int tempi = commandphrase.size()-1;
	while(commandphrase[tempi] == ' '){
		tempi--;
	}
	
	commandphrase = commandphrase.substr(0, tempi+1);
	vector<string> parsedcommand;
	parsedcommand = parseCommand(commandphrase);
	
	for(int i = 1; i < parsedcommand.size(); i++){
		if(parsedcommand[i][0] == '~'){
			parsedcommand[i] = homePath + parsedcommand[i].substr(1, parsedcommand[i].size()-1);
		}
	}
	
	logstr = commandphrase;
	
	if(parsedcommand[0] == "goto"){
		if(parsedcommand.size() == 2) gotodir(parsedcommand[1]);
	}
	else if(parsedcommand[0] == "search"){
		if(parsedcommand.size() == 2){
			if(search(parsedcommand[1])){
				cout << "True" << endl;
			}
			else cout << "False" << endl;
		}
	}
	else if(parsedcommand[0] == "delete_file"){
		if(parsedcommand.size() == 2) del(parsedcommand[1]);
		else logstr = "Invalid command";
	}
	else if(parsedcommand[0] == "delete_dir"){
		if(parsedcommand.size() == 2) del(parsedcommand[1]);
		else logstr = "Invalid command";
	}
	else if(parsedcommand[0] == "create_file"){
		if(parsedcommand.size() == 3) create_file(parsedcommand[1], parsedcommand[2]);
		else logstr = "Invalid command";
	}
	else if(parsedcommand[0] == "create_dir"){
		if(parsedcommand.size() == 3) create_dir(parsedcommand[1], parsedcommand[2]);
		else logstr = "Invalid command";
	}
	else if(parsedcommand[0] == "copy"){
		if(parsedcommand.size() >= 3){
			string dest = parsedcommand[parsedcommand.size()-1];
			for(int i = 1; i < parsedcommand.size()-1; i++){
				copy(parsedcommand[i], dest);
			}
		}
		else logstr = "Invalid command";
	}
	else if(parsedcommand[0] == "move"){
		if(parsedcommand.size() >= 3){
			string dest = parsedcommand[parsedcommand.size()-1];
			for(int i = 1; i < parsedcommand.size()-1; i++){
				move(parsedcommand[i], dest);
			}
		}
		else logstr = "Invalid command";
	}
	else if(parsedcommand[0] == "rename"){
		if(parsedcommand.size() == 3) myrename(parsedcommand[1], parsedcommand[2]);
		else logstr = "Invalid command";
	}
	else if(parsedcommand[0] == "exit" || parsedcommand[0] == "quit" || parsedcommand[0] == "close"){
		exit(0);
	}
	else{
		logstr = "Invalid command";
	}
}

//Process button presses in normal mode
void keypressprocess(){
	int nread;
	char c;
	int rowmin;
	int yoffsetmax;
	if((winrow - bottomlimit) < outputBuffer.size()){
		rowmin = winrow - bottomlimit;
		yoffsetmax = outputBuffer.size() - winrow + bottomlimit;
	}
	else{
		yoffsetmax = 0;
		rowmin = outputBuffer.size();
	}
	
	while((nread = read(STDIN_FILENO, &c, 1)) != 1){
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	
	if(c == '\x1b'){
		char seq[3];
		
		if(read(STDIN_FILENO, &seq[0], 1) != 1) return;
		if(read(STDIN_FILENO, &seq[1], 1) != 1) return;
		
		if(seq[0] == '['){
			switch(seq[1]){
				case 'A':
					if(cursory>1)cursory--;
					else if(cursory <= 1 && yoffset >= 1){
						yoffset--;
						cursory = 1;
					}
					break;
				case 'B': 
					if(cursory<rowmin)cursory++;
					else if(cursory>=rowmin && yoffset < yoffsetmax){
						yoffset++;
						cursory = rowmin;
					}
					break;
				case 'C':
					cursory = 1;
					cursorx = 1;
					yoffset = 0;
					xoffset = 0;
					gonextdir();
					break;
				case 'D':
					cursory = 1;
					cursorx = 1;
					yoffset = 0;
					xoffset = 0;
					goprevdir();
					break;
			}
		}
	}
	
	else if(c == 10){
		if(cursory <= dirlist.size()){
			struct stat pathDetails;
			if((lstat(dirlist[yoffset+cursory-1].c_str(), &pathDetails)) == -1){
				//cout << "Invalid path" << endl;
				return;
			}
			if(S_ISDIR(pathDetails.st_mode)){
				gotodirN(yoffset+cursory-1);
				cursory = 1;
				cursorx = 1;
				yoffset = 0;
				xoffset = 0;
			}
			else if(S_ISREG(pathDetails.st_mode)){
				int pid = fork();
				if (pid == 0){
				    execl(EXEC_PATH, EXEC_NAME, dirlist[cursory-1].c_str(), (char *)0);
				    exit(1);
				}
			}
		}
	}
	
	else if(c == 'q' && quitflag == 1){
		exit(0);
	}
	
	else if(c == 'a'){
		if (xoffset >= 1) xoffset--;
	}
	
	else if(c == 'd'){
		xoffset++;
	}
	
	else if(c == 'h'){
		cursory = 1;
		cursorx = 1;
		yoffset = 0;
		xoffset = 0;
		gotodir(homePath);
	}
	
	else if(c == 'l'){
		if(yoffset < yoffsetmax){
			yoffset++;
		}
	}
	
	else if(c == 'k'){
		if(yoffset >= 1){
			yoffset--;
		}
	}
	
	else if(c == 127){
		cursory = 1;
		cursorx = 1;
		yoffset = 0;
		xoffset = 0;
		gotodir("..");
	}
	
	else if(c == ':'){
		setCommandMode();
		refreshScreen();
	}
}

void keypressprocessC(){
	int nread;
	char c;

	while((nread = read(STDIN_FILENO, &c, 1)) != 1){
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	if(c == 'q' && quitflag == 1){
		exit(0);
	}
	else if(c == 27){
		setNormalMode();
		quitflag = 1;
		commandBuffer = "";
	}
	else if(c == 10){
		cursory = 1;
		cursorx = 1;
		yoffset = 0;
		xoffset = 0;
		quitflag = 1;
		
		processCommand(commandBuffer);
		refreshScreen();
		commandBuffer = "";
	}
	else{
		quitflag = 0;
		commandBuffer.push_back(c);
	}
}

/*----------------------------------------------

NAVIGATION

-----------------------------------------------*/

void driver(){
	int num;
	char* c_buffer;
	string c;
	while(true){
		if(!commandMode) refreshScreen();
		if(!commandMode) keypressprocess();
		else keypressprocessC();
	}
}

/*----------------------------------------------

WINDOW RESIZE SIGNAL HANDLING

-----------------------------------------------*/

static void sig_handler(int sig){
	if (SIGWINCH == sig) {
		updateWindowSize();
		refreshScreen();
	}
}

int main(){
	//Environment preperation
	signal(SIGWINCH, sig_handler);
	prepareTerminalInfo();
	updateWindowSize();
	setNormalMode();
	setHomeDir();
	gotodir(".");
	
	driver();
	
	return 0;
}


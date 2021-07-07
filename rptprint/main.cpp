#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h> /* _beginthread, _endthread */
#include <vector>

#include "main.h"
#include "INIReader.h"

/***[Node Stuff]********************************************/
NODE_VALUE *initNode(char *v)
{
	NODE_VALUE *ptr;
	ptr = (NODE_VALUE *) calloc(1, sizeof(NODE_VALUE)); /* calloc will zero-out the char[] */
	if( ptr == NULL )
		return (NODE_VALUE *) NULL;
	else {
		sprintf_s(ptr->arr, _MAX_FNAME, "%s", v);
		return ptr;
	}	
}

void pruneVector()
{
	std::vector<int> RemoveRecords;
	unsigned int h = 0;
	for( std::vector<struct node_value*>::iterator i = nodes.begin(); i != nodes.end(); ++i )
	{
		// check if the node arr value is blank.
		if( ((*i)->arr)[0] == '\0' )
		{
			// Used node, we can remove it.
			RemoveRecords.push_back(h);
		}
		++h;
	}

	// Now remove them in reverse order to preserve integrity
	while( !RemoveRecords.empty() )
	{
		h = RemoveRecords.back();
		if( nodes.size() - 1 > h )
		{
			free(nodes[h]);
			nodes.erase(nodes.begin() + h);
		}
		RemoveRecords.pop_back();
	}
	RemoveRecords.clear();
}

void cleanVector()
{
	int x = nodes.size() - 1;
	while( x > 0 && !nodes.empty() )
	{
		free(nodes[x]);
		nodes.erase(nodes.begin() + x);
		--x;
	}
}

void child_process(void *value_arr) //NODE_VALUE *value_arr)
{
	char temp_buffer[6000];
	
	// Print the command to the screen.
	memset(temp_buffer, '\0', sizeof(temp_buffer));
	sprintf_s(temp_buffer, 6000, "%s %s", watchCmd, ((NODE_VALUE*)value_arr)->arr);
	
	// Doesn't 100% work with threading.
	//dprintf("child_process: %s\n", temp_buffer);
	
	// Run the command
	system(temp_buffer);
	
	// Zero out the command
	memset(((NODE_VALUE*)value_arr)->arr, '\0', sizeof(((NODE_VALUE*)value_arr)->arr));

	// Kill the thread
	_endthread();
}
/***********************************************************/

// Read the INI file, grab the first section in the file and pull the WATCHDIR setting.
// WATCHDIR is the directory that needs to be watched for *.WAVE files.
bool ReadINI()
{
	INIReader reader(inifile);
	std::string watch;
	std::string cmd, ext;
	
	// Check for file error
	if( reader.ParseError() < 0 )
	{
		dprintf("Unable to load INI file: %s\n", inifile);
		return false;
	}
	
	// Get the watch directory setting
	watch = reader.Get("settings", "watchdir", "watch");
	if( watch.size() > 4096 )
	{
		dprintf("Watch directory value is TOO LARGE (4096 char limit)\n");
		return false;
	}
	
	// Copy from string class to char array
	std::copy(watch.begin(), watch.end(), watchDir);
	watchDir[watch.size()] = '\0';

	//Get the watch command setting
	cmd = reader.Get("settings", "watchcmd", "echo");
	if( cmd.size() > 4096 )
	{
		dprintf("Watch command value is TOO LARGE (4096 char limit)\n");
		return false;
	}

	// Copy from string class to char array
	std::copy(cmd.begin(), cmd.end(), watchCmd);
	watchCmd[cmd.size()] = '\0';

	ext = reader.Get("settings", "watchext", ".WAVE");
	if( ext.size() > 50 )
	{
		dprintf("Watch extension value is TOO LARGE (50 char limit)\n");
		return false;
	}

	// Create the finder string
	strncat_s(watchFiles, watchDir, strnlen(watchDir, 4096));
	strncat_s(watchFiles, ext.c_str(), ext.size());

	verbose = reader.GetBoolean("settings", "verbose", false);

	dprintf("Watch Directory: %s\n", watchDir);
	dprintf("Command: %s\n", watchCmd);
	return true;
}

void RefreshDirectory(char *lpDir)
{
    WIN32_FIND_DATAA findFiles;
	HANDLE hFind;
	struct node_value *r;
	char file[_MAX_FNAME];
	char ext[_MAX_EXT];
	
	dprintf("Checking Directory %s...\n", lpDir);
	
	// Find the first matching file.
	hFind = FindFirstFileA(watchFiles, &findFiles);	
	if( hFind == INVALID_HANDLE_VALUE )
	{
		dprintf("No Files Found\n");
		return;
	}

	while( hFind != INVALID_HANDLE_VALUE )
	{
		
		// Split the filename from the extension.
		_splitpath_s(findFiles.cFileName, NULL, 0, NULL, 0, file, _MAX_FNAME, ext, _MAX_EXT);

		// Not safe.
		//if( strlen(findFiles.cFileName) > 5 )
		//	findFiles.cFileName[strlen(findFiles.cFileName)-5] = '\0'; // hack job

		// Create new node
		if( strlen(file) > 0 )
			r = initNode(file);

		// Add to stack
		nodes.push_back(r);

		// New Child Thread
		_beginthread(child_process, 0, (void*)(r));

		// Find next file, if exists
		if( !FindNextFileA(hFind, &findFiles) )
		{
			// if not exist, close finder and set handle to invalid.
			FindClose(hFind);
			hFind = INVALID_HANDLE_VALUE;
		}
	}
}

void WatchDirectory(char *lpDir)
{
	DWORD dwWaitStatus; 
	HANDLE dwChangeHandles;
 
	// Trigger on last file write. (Does not support renames, just file write/creation)
	dwChangeHandles = FindFirstChangeNotificationA
		(
			lpDir,
			FALSE,
			FILE_NOTIFY_CHANGE_LAST_WRITE
		);
 
	if( dwChangeHandles == INVALID_HANDLE_VALUE || dwChangeHandles == NULL )
	{
		dprintf("\n ERROR: FindFirstChangeNotification function failed.\n");
		return;
	}
 
	while(1)
	{
		// Remove any old entries.
		pruneVector();

		// this waits until something triggers the change notification
		dwWaitStatus = WaitForSingleObject(dwChangeHandles, INFINITE);

		switch (dwWaitStatus) 
		{ 
			case WAIT_OBJECT_0:
				// Check Directory for filenames and then create new processes.
				RefreshDirectory(lpDir);
				if( FindNextChangeNotification(dwChangeHandles) == FALSE )
				{
					dprintf("Fatal Error, FindNextChangeNotification function failed.\n");
					return;
				}
				break;
			case WAIT_TIMEOUT:
				// This shouldn't happen because of the INFINITE wait
				break;
			default:
				// Something very bad happened, exit
				dprintf("Fatal Error, WaitForSingleObject returned an unhandled status\n");
				return;
				break;
		}
	}
}

int main()
{
	dprintf("Starting Service.\n\n");
	if( !ReadINI() )
		dprintf("Fatal Error, Exiting.\n");
	WatchDirectory(watchDir);
	
	// Should not get here until waitforsingleobject fails
	dprintf("Cleaning Vectors\n");
	cleanVector();

	dprintf("Exiting.\n");
	return 0;
}
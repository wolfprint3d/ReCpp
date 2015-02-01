#pragma once
#include <string>
#include <vector>

enum IOFlags {
	READONLY,			// opens an existing file for reading
	READONLYEXECUTE,	// open an existing file for read & execute
	READWRITE,			// opens an existing file for read/write
	READWRITECREATE,	// creates or opens an existing file for read/write
	CREATENEW,			// creates new file for writing
	CREATETEMP,			// creates a temporary file, data is not flushed to disk and File is deleted on close
};


#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif


/**
 * Automatic whole file loading buffer
 */
struct load_buffer
{
	char* Buffer;  // dynamic or internal buffer
	int   Size;    // buffer size in bytes

	inline load_buffer() : Buffer(0), Size(0) {}
	// takes ownership of a malloc-ed pointer and frees it when out of scope
	inline load_buffer(char* buffer, int size) : Buffer(buffer), Size(size) {}
	~load_buffer();

	load_buffer(const load_buffer& rhs)            = delete; // NOCOPY
	load_buffer& operator=(const load_buffer& rhs) = delete; // NOCOPY

	load_buffer(load_buffer&& mv);
	load_buffer& operator=(load_buffer&& mv);

	// acquire the data pointer of this load_buffer, making the caller own the buffer
	char* steal_ptr();

	template<class T> inline operator T*() { return (T*)Buffer; }
	inline int size()      const { return Size; }
	inline char* data()    const { return Buffer; }
	inline operator bool() const { return Buffer != nullptr; }
};




/**
* Buffered FILE structure for performing random access read/write
*
*  Example usage:
*         file f("test.obj");
*         char* buffer = malloc(f.size());
*         f.read(buffer, f.size());
*
*/
struct file
{
	void*	Handle;	// File handle
	IOFlags	Mode;	// File openmode READWRITE or READONLY


	inline file() : Handle(0), Mode(READONLY)
	{
	}

	/**
	* Opens an existing file for reading with mode = READONLY
	* Creates a new file for reading/writing with mode = READWRITE
	* @param filename File name to open or create
	* @param mode File open mode
	*/
	file(const char* filename, IOFlags mode = READONLY);
	file(const std::string& filename, IOFlags mode = READONLY);
	file(file&& f);
	~file();

	file& operator=(file&& f);

	file(const file& f)            = delete;
	file& operator=(const file& f) = delete;
public:

	/**
	* Opens an existing file for reading with mode = READONLY
	* Creates a new file for reading/writing with mode = READWRITE
	* @param filename File name to open or create
	* @param mode File open mode
	* @return TRUE if file open/create succeeded, FALSE if failed
	*/
	bool open(const char* filename, IOFlags mode = READONLY);
	inline bool open(const std::string& filename, IOFlags mode = READONLY)
	{
		return open(filename.c_str(), mode);
	}
	void close();

	/**
	* @return TRUE if file handle is valid (file exists or has been created)
	*/
	bool good() const;
	inline operator bool() const { return good(); }

	/**
	* @return TRUE if the file handle is INVALID
	*/
	bool bad() const;

	/**
	* @return Size of the file in bytes
	*/
	int size() const;

	/**
	* @return Long size of file in bytes
	*/
	unsigned __int64 sizel() const;

	/**
	* Reads a block of bytes from the file. Standard OS level 
	* IO buffering is performed.
	* 
	* @param buffer Buffer to read bytes to
	* @param bytesToRead Number of bytes to read from the file
	* @return Number of bytes actually read from the file
	*/
	int read(void* buffer, int bytesToRead);

	/**
	* Reads the entire contents of the file into a load_buffer
	* unbuffered_file is used internally
	*/
	load_buffer read_all();

	/**
	* Reads the entire contents of the file into a load_buffer
	* The file is opened as READONLY, unbuffered_file is used internally
	*/
	static load_buffer read_all(const char* filename);
	inline static load_buffer read_all(const std::string& filename)
	{
		return read_all(filename.c_str());
	}

	/**
	* Writes a block of bytes to the file. Regular Windows IO
	* buffering is ENABLED for WRITE.
	*
	* @param buffer Buffer to write bytes from
	* @param bytesToWrite Number of bytes to write to the file
	* @return Number of bytes actually written to the file
	*/
	int write(const void* buffer, int bytesToWrite);

	/**
	* Creates a new file and fills it with the provided data.
	* Regular Windows IO buffering is ENABLED for WRITE.
	*
	* Openmode is IOFlags::CREATENEW
	*
	* @param filename Name of the file to create and write to
	* @param buffer Buffer to write bytes from
	* @param bytesToWrite Number of bytes to write to the file
	* @return Number of bytes actually written to the file
	*/
	static int write_new(const char* filename, const void* buffer, int bytesToWrite);

	inline static int write_new(const std::string& filename, const void* buffer, int bytesToWrite)
	{
		return write_new(filename.c_str(), buffer, bytesToWrite);
	}

	/**
	* Seeks to the specified position in a file. Seekmode is 
	* determined like in fseek: SEEK_SET, SEEK_CUR and SEEK_END
	*
	* @param filepos Position in file where to seek to
	* @param seekmode Seekmode to use: SEEK_SET, SEEK_CUR or SEEK_END
	* @return Current position in the file
	*/
	int seek(int filepos, int seekmode = SEEK_SET);

	/**
	* @return Current position in the file
	*/
	int tell() const;

	/**
	* @return File creation time
	*/
	unsigned __int64 time_created() const;

	/**
	* @return File access time - when was this file last accessed?
	*/
	unsigned __int64 time_accessed() const;

	/**
	* @return File write time - when was this file last modified
	*/
	unsigned __int64 time_modified() const;
};




bool file_exists(const char* filename);
bool file_exists(const std::string& filename);
bool file_exists(const char* filename, int length);

bool folder_exists(const char* folder);
bool folder_exists(const std::string& folder);
bool folder_exists(const char* folder, int length);

unsigned int file_size(const char* file);
unsigned int file_size(const std::string& file);
unsigned int file_size(const char* filename, int length);

unsigned __int64 file_sizel(const char* filename);
unsigned __int64 file_sizel(const std::string& filename);
unsigned __int64 file_sizel(const char* filename, int length);

time_t file_modified(const char* filename);
time_t file_modified(const std::string& filename);
time_t file_modified(const char* filename, int length);

bool create_folder(const char* filename);
bool create_folder(const std::string& file);
bool create_folder(const char* filename, int length);

bool delete_folder(const char* filename);
bool delete_folder(const std::string& file);
bool delete_folder(const char* filename, int length);

/**
* @brief Static container for directory utility functions
*/
struct path
{
	static int list_dirs(std::vector<std::string>& out, const char* directory, const char* matchPattern = "*");
	static int list_files(std::vector<std::string>& out, const char* directory, const char* matchPattern = "*.*");

	/**
	* @return The current working directory of the application
	*/
	static std::string get_working_dir();

	/**
	* @brief Set the working directory of the application to a new value
	*/
	static void set_working_dir(const std::string& new_wd);

	/**
	*  @brief Transform a relative path to a full path name
	*/
	static std::string fullpath(const std::string& relativePath);

	/**
	*  @brief Transform a relative path to a full path name
	*/
	static std::string fullpath(const char* relativePath);

	/**
	* @brief Extract the filename from a path name
	*/
	static std::string filename(const std::string& someFilePath);

	/**
	* @brief Extract the filename from a path name
	*/
	static std::string filename(const char* someFilePath);

	/**
	* @brief Extract the foldername from a path name
	*/
	static std::string foldername(const std::string& someFolderPath);

	/**
	* @brief Extract the foldername from a path name
	*/
	static std::string foldername(const char* someFolderPath);
};




/**
* @brief Dirwatch filtering flags
*/
enum dirwatch_flags
{
	notify_filename_change = 0x001, // a file name has changed
	notify_dirname_change  = 0x002, // a directory name has changed
	notify_attrib_change   = 0x004, // a file attribute has changed
	notify_filesize_change = 0x008, // a file size has changed
	notify_file_modified   = 0x010, // a file was modified
	notify_file_accessed   = 0x020, // a file was accessed
	notify_file_created    = 0x040, // a file was created
	notify_security_change = 0x100, // a file security-descriptor was changed
};


/**
* @brief Simple method of checking directory changes at OS level,
*        meaning low overhead, high performance
* @note  There is no method to monitor which specific files changed
*/
struct dirwatch
{
	void* Handle;

	/**
	* @brief Creates an uninitialized dirwatch object
	*/
	inline dirwatch() : Handle(0) {}

	/**
	* @brief Start monitoring the specified directory
	* @param folder Name of the directory to monitor
	* @param flags [notify_filename_change] Directory watch flags to combine
	* @param monitorSubDirs [false] Set this TRUE if you also wish to monitor all subdirectories
	*/
	dirwatch(const char* folder, dirwatch_flags flags = notify_filename_change, bool monitorSubDirs = false);

	/**
	* @brief Start monitoring the specified directory
	* @param folder Name of the directory to monitor
	* @param flags [notify_filename_change] Directory watch flags to combine
	* @param monitorSubDirs [false] Set this TRUE if you also wish to monitor all subdirectories
	*/
	dirwatch(const std::string& folder, dirwatch_flags flags = notify_filename_change, bool monitorSubDirs = false);

	~dirwatch();

	/**
	* @brief Closes the dirwatch object
	*/
	void close();

	/**
	* @brief Start monitoring the specified directory
	* @param folder Name of the directory to monitor
	* @param flags [notify_filename_change] Directory watch flags to combine
	* @param monitorSubDirs [false] Set this TRUE if you also wish to monitor all subdirectories
	*/
	void initialize(const char* folder, dirwatch_flags flags = notify_filename_change, bool monitorSubDirs = false);

	/**
	* @brief Start monitoring the specified directory
	* @param folder Name of the directory to monitor
	* @param flags [notify_filename_change] Directory watch flags to combine
	* @param monitorSubDirs [false] Set this TRUE if you also wish to monitor all subdirectories
	*/
	void initialize(const std::string& folder, dirwatch_flags flags = notify_filename_change, bool monitorSubDirs = false);

	/**
	* @brief Waits until directory is modified or until specified timeout
	* @param timoutMillis [INFINITE -1] Specify timeout for wait operation
	* @return TRUE if directory was modified, FALSE if timeout
	*/
	bool wait(int timeoutMillis = -1) const;

	/**
	* @brief Checks if the directory was modified recently
	* @return TRUE if directory was modified, FALSE if no change happened
	*/
	bool changed() const;
};



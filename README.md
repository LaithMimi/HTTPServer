

#**HTTP Server with Thread Pool**

## **Overview**
This project implements a multithreaded HTTP server capable of handling client requests over TCP connections. The server adheres to a subset of the HTTP/1.0 specification, providing responses for basic file and directory requests while handling multiple connections using a thread pool.

---

## **Features**
1. **HTTP Protocol Support**: 
   - Responds to `GET` requests.
   - Sends appropriate HTTP status codes (`200 OK`, `404 Not Found`, etc.).
2. **Thread Pool**: 
   - Efficiently manages client connections using a fixed pool of threads.
3. **Directory and File Handling**:
   - Supports directory listings.
   - Redirects requests for directories without trailing slashes.
   - Serves files with appropriate MIME types based on file extensions.
4. **Error Handling**:
   - Handles invalid HTTP requests with appropriate error codes (e.g., `400 Bad Request`).
   - Responds to unsupported methods with `501 Not Implemented`.

---

## **Files in the Project**
- **`server.c`**:
  - Implements the main HTTP server functionality.
  - Listens for connections and dispatches tasks to the thread pool.
- **`threadpool.c`**:
  - Implements a thread pool to handle client requests efficiently.
  - Includes task queuing and thread management logic.
- **`threadpool.h`**:
  - Header file for the thread pool implementation.
- **`Makefile`** *(optional)*:
  - Includes compilation instructions for the server.
- **`README`**:
  - This file, providing an overview of the project and usage instructions.

---

## **Usage**
### **Compiling the Server**
To compile the server, use the following command:
```bash
gcc -o server server.c threadpool.c -lpthread
```

### **Running the Server**
The server requires the following command-line arguments:
```bash
./server <port> <pool-size> <max-queue-size> <max-number-of-requests>
```

- `<port>`: Port number the server will listen on.
- `<pool-size>`: Number of threads in the thread pool.
- `<max-queue-size>`: Maximum size of the task queue.
- `<max-number-of-requests>`: Maximum number of client requests the server will handle.

**Example:**
```bash
./server 8080 4 10 100
```
This command starts the server on port 8080 with 4 threads, a queue size of 10, and a limit of 100 requests.

---

## **Testing the Server**
1. **Browser Testing**:
   - Navigate to `http://localhost:<port>` in your web browser to view the server's response.
2. **Command-Line Tools**:
   - Use `curl` to test specific endpoints:
     ```bash
     curl -v http://localhost:<port>/<path>
     ```
3. **Directory Listing**:
   - If a directory is requested, the server provides a listing of its contents or serves `index.html` if present.

---

## **HTTP Responses**
### **Supported Status Codes**:
- `200 OK`: Request was successful.
- `302 Found`: Redirect for directory requests without a trailing slash.
- `400 Bad Request`: The client sent an invalid HTTP request.
- `403 Forbidden`: Access to the requested file or directory is denied.
- `404 Not Found`: The requested resource does not exist.
- `500 Internal Server Error`: Server-side error.
- `501 Not Supported`: Method not supported (e.g., non-`GET` requests).

---

## **Error Handling**
1. **Server Start-Up Errors**:
   - Invalid command-line arguments.
   - Failure to bind to the specified port.
2. **Client Errors**:
   - Invalid HTTP requests are rejected with `400 Bad Request`.
   - Requests for unsupported methods return `501 Not Supported`.
3. **File Errors**:
   - Missing files return `404 Not Found`.
   - Files without proper read permissions return `403 Forbidden`.

---

## **Project Structure**
- **`server.c`**:
  - Contains the main server loop, connection handling, and response construction.
- **`threadpool.c`**:
  - Handles task queuing, thread creation, and execution of client requests.
- **`threadpool.h`**:
  - Declares structures and functions for the thread pool.
- **HTTP Response Templates**:
  - Embedded within the code for efficiency.

---

## **Known Limitations**
- Only supports `GET` requests.
- Assumes a maximum request length of 4000 bytes for the first line.
- Directory listings assume each entry is less than 500 bytes.

---

## **Compilation Notes**
- Ensure the `-lpthread` flag is used during compilation to link the pthread library.
- Name the executable as `server`.

---

## **Future Enhancements**
- Add support for more HTTP methods (`POST`, `PUT`, etc.).
- Implement persistent connections (HTTP/1.1).
- Improve logging for debugging and performance monitoring.




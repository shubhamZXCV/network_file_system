# Assumptions for Distributed Network File System Code

## 1. Write Operations
- **WRITE Operation**: This operation allows input directly from the terminal and can handle a single line of text up to **1024 bytes**. It is intended for smaller writes where the user inputs data interactively.
- **LARGE Operation**: For larger data writes, a file named `LARGE.txt` is used. The contents of `LARGE.txt` are written to the storage server at the desired location specified in the request. This method is designed for larger data transfers and requires pre-written data in the `LARGE.txt` file.
- **Asynchronous Write**: When the total bytes to be transferred exceed **8192 bytes**, the operation switches to an **asynchronous write** mode. This ensures that large data writes are handled efficiently without blocking the main thread.

## 2. Naming Server Requirements
- The **Naming Server** must always be operational for the distributed file system to function correctly. It is assumed to be the **default storage server 0**, acting as the primary coordinator for file and directory management.
- The **IP address** of the Naming Server is defined as a **macro** within the code and will be changed on the spot during the presentation to demonstrate flexibility and adaptability in different network setups.

## 3. Copy Command Behavior
- **Directory to File**: Copying from a directory to a file is **not permitted**.
- **File to File**: Both source and destination files must already exist for the operation to succeed. Additionally, the copy operation for files has a data transfer **threshold of 1024 bytes**.
- **Directory to Directory**: Only the **contents** of the directory are copied to the destination. The directory itself is not recreated or duplicated at the destination, ensuring only its immediate files are transferred.

## 4. Audio Streaming
- **Streaming Constraints**: Once a client initiates an audio stream, they **cannot stop** the stream. The audio will continue until it is complete, ensuring uninterrupted playback once started.

## 5. Data Structure for File/Directory Storage
- **Trie-Based Storage**: All files and directories in the system are stored in the form of a **trie structure**. This approach ensures that each file or directory path is unique and prevents naming conflicts within the system.

> client connect to server , operation is performed and then the connection breaks.

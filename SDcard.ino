//
////***********************************************************************************************
////*
////*   list dir
////*
////***********************************************************************************************
//void listDir(fs::FS & fs, const char * dirname, uint8_t levels) {
//  SerialMon.printf("Listing directory: % s\n", dirname);
//
//  File root = fs.open(dirname);
//  if (!root) {
//    SerialMon.println("Failed to open directory");
//    return;
//  }
//  if (!root.isDirectory()) {
//    SerialMon.println("Not a directory");
//    return;
//  }
//
//  File file = root.openNextFile();
//  while (file) {
//    if (file.isDirectory()) {
//      SerialMon.print("  DIR : ");
//      SerialMon.println(file.name());
//      if (levels) {
//        listDir(fs, file.name(), levels - 1);
//      }
//    } else {
//      SerialMon.print("  FILE: ");
//      SerialMon.print(file.name());
//      SerialMon.print("  SIZE: ");
//      SerialMon.println(file.size());
//    }
//    file = root.openNextFile();
//  }
//}
////***********************************************************************************************
////*
////*   create dir
////*
////***********************************************************************************************
//void createDir(fs::FS & fs, const char * path) {
//  SerialMon.printf("Creating Dir: % s\n", path);
//  if (fs.mkdir(path)) {
//    SerialMon.println("Dir created");
//  } else {
//    SerialMon.println("mkdir failed");
//  }
//}
////***********************************************************************************************
////*
////*   remove dir
////*
////***********************************************************************************************
//void removeDir(fs::FS & fs, const char * path) {
//  SerialMon.printf("Removing Dir: % s\n", path);
//  if (fs.rmdir(path)) {
//    SerialMon.println("Dir removed");
//  } else {
//    SerialMon.println("rmdir failed");
//  }
//}
////***********************************************************************************************
////*
////*   read file
////*
////***********************************************************************************************
//void readFile(fs::FS & fs, const char * path) {
//  SerialMon.printf("Reading file: % s\n", path);
//
//  File file = fs.open(path);
//  if (!file) {
//    SerialMon.println("Failed to open file for reading");
//    return;
//  }
//
//  SerialMon.print("Read from file : ");
//  while (file.available()) {
//    SerialMon.write(file.read());
//  }
//  file.close();
//}
////***********************************************************************************************
////*
////*   write file
////*
////***********************************************************************************************
//void writeFile(fs::FS & fs, const char * path, const char * message) {
//  SerialMon.printf("Writing file : % s\n", path);
//
//  File file = fs.open(path, FILE_WRITE);
//  if (!file) {
//    SerialMon.println("Failed to open file for writing");
//    return;
//  }
//  if (file.print(message)) {
//    SerialMon.println("File written");
//  } else {
//    SerialMon.println("Write failed");
//  }
//  file.close();
//}
////***********************************************************************************************
////*
////*   append file
////*
////***********************************************************************************************
//void appendFile(fs::FS & fs, const char * path, const char * message) {
//  SerialMon.printf("Appending to file : % s\n", path);
//
//  File file = fs.open(path, FILE_APPEND);
//  if (!file) {
//    SerialMon.println("Failed to open file for appending");
//    return;
//  }
//  if (file.print(message)) {
//    SerialMon.println("Message appended");
//  } else {
//    SerialMon.println("Append failed");
//  }
//  file.close();
//}
////***********************************************************************************************
////*
////*   rename file
////*
////***********************************************************************************************
//void renameFile(fs::FS & fs, const char * path1, const char * path2) {
//  SerialMon.printf("Renaming file % s to % s\n", path1, path2);
//  if (fs.rename(path1, path2)) {
//    SerialMon.println("File renamed");
//  } else {
//    SerialMon.println("Rename failed");
//  }
//}
////***********************************************************************************************
////*
////*   delete file
////*
////***********************************************************************************************
//void deleteFile(fs::FS & fs, const char * path) {
//  SerialMon.printf("Deleting file : % s\n", path);
//  if (fs.remove(path)) {
//    SerialMon.println("File deleted");
//  } else {
//    SerialMon.println("Delete failed");
//  }
//}

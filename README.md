# File-Partition-Parser

This is a fun side project of mine that can actually be quite useful.

I’ve also created a GUI application for this, to make it more user-friendly and to include some extra features.
You can check it out here: https://github.com/jonaspfl/file-partitioner.

The tool is available for both macOS and Windows.
This application can be used to partition the data of an arbitrary number of files into data files, that contain a specified maximum number of bytes each.
The application operates in two modes:
- **encode**: _split and encode files into data partitions._
- **decode**: _reassemble the original files from the data partitions._

This tool can be used to overcome maximum file sizes regarding uploads or similar things, when there is an exact file size limit.

Also, this was completely coded in C because I am stupid.

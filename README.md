# FilePartitionParser

## General Information

This is a fun side project of mine that can actually be quite useful.

I’ve also created a GUI application for this, to make it more user-friendly and to include some extra features.
You can check it out here: [FilePartitioner](https://github.com/jonaspfl/file-partitioner).

NOTE: This application does not provide any GUI by itself. It needs to be executed through the terminal (instructions below). Otherwise, you need to additionally use the GUI application mentioned above.

The tool is available for both macOS and Windows.
This application can be used to partition the data of an arbitrary number of files into data files, that contain a specified maximum number of bytes each.
The application operates in two modes:
- **encode**: _split and encode files into data partitions._
- **decode**: _reassemble the original files from the data partitions._

This tool can be used to overcome maximum file sizes regarding uploads or similar things, when there is an exact file size limit.

Also, this was completely coded in C because I am stupid.

## How To Use
Execute the program using `./parser` (macOS) or `./parser.exe` (Windows) through the terminal.

### Syntax
Like mentioned above, the parser can be executed in two modes.

#### encode:
`./parser encode <max output filesize> <output filename> <input filename 1> ... <input filename n>`
- `<max output filesize>`: The maximum size of the resulting data-files. Input as _number_ and one of the letters _K/M/G_. Examples: `3K -> 3KiB`, `7M -> 7MiB`, `1G -> 1GiB` or `0 -> infinite`
- `<output filename>`: The filename you want the resulting data files named as.
- `<input filename 1> ... <input filename n>`: The one or more input files, that you want to encode.

#### decode:
`decode <input filename>`
- `<input filename>`: The _main file_ that corresponds to the `_data` files, that you want to decode.
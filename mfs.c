// The MIT License (MIT)
// 
// Copyright (c) 2020 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

/*
 * Muhammad Zaharudin
 * 1001835444
 */


#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_NUM_ARGUMENTS 5

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

struct __attribute__((__packed__)) DirectoryEntry
{
  char DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];

char BS_OEMName[8];
int16_t BPB_BytesPerSec;
int8_t BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t BPB_NumFATs;
int16_t BPB_RootEntCnt;
char BS_VolLab[11];
int32_t BPB_FATSz32;
int32_t BPB_RootClus;

int32_t RootDirSectors = 0;
int32_t FirstDataSector = 0;
int32_t FirstSectorofCluster = 0;

FILE *fp;

int fileOpened = 0;
char placeHolder[11];
uint8_t attrPlaceHolder;


int LBAToOffset(int sector)
{
  return ((sector - 2) * BPB_BytesPerSec) +
         (BPB_BytesPerSec * BPB_RsvdSecCnt) +
         (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec);
}

int compare(char* imageName, char* inp)
{
    char IMG_Name[12]; 
    strncpy(IMG_Name, imageName, 12);

    char input[11]; 
    strncpy(input, inp, 11);

    char expanded_name[12];
    memset( expanded_name, ' ', 12 );

    char *token = strtok( input, "." );

    strncpy( expanded_name, token, strlen( token ) );

    token = strtok( NULL, "." );

    if( token )
    {
      strncpy( (char*)(expanded_name+8), token, strlen(token ) );
    }

    expanded_name[11] = '\0';

    int i;
    for( i = 0; i < 11; i++ )
    {
      expanded_name[i] = toupper( expanded_name[i] );
    }

    if( strncmp( expanded_name, IMG_Name, 11 ) == 0 )
    {
      return 0;
    }
    
    return 1;
}

int16_t NextLB(uint32_t sector)
{
    uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector * 4);
    int16_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 2, 1, fp);
    return val;
}

int main()
{

    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

    while( 1 )
    {
        // Print out the mfs prompt
        printf ("mfs> ");

        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

        
        char *token[MAX_NUM_ARGUMENTS];

        int   token_count = 0;                                 

        // Pointer to point to the token
        // parsed by strsep
        char *arg_ptr;                                         

        char *working_str  = strdup( cmd_str );                

        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;

        // Tokenize the input stringswith whitespace used as the delimiter
        while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
                  (token_count<MAX_NUM_ARGUMENTS))
        {
          token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
          if( strlen( token[token_count] ) == 0 )
          {
            token[token_count] = NULL;
          }
            token_count++;
        }

        // Now print the tokenized input as a debug check
        // \TODO Remove this code and replace with your FAT32 functionality

        free( working_root );

         //Continues if there is no input
        if (token[0] == NULL)
        {
          continue;
        }

        if (strcmp("open", token[0]) == 0)
        {
            fp = fopen(token[1], "r");
            
            if (fp == NULL)
            {
              printf("Error: File system image not found.\n");
              continue;
            }
            
            else if (fileOpened == 1)
            {
              printf("Error: File system image already open.\n");
              continue;
            }
            else
            {
                fseek(fp, 11, SEEK_SET);
                fread(&BPB_BytesPerSec, 2, 1, fp);

                fseek(fp, 13, SEEK_SET);
                fread(&BPB_SecPerClus, 1, 1, fp);

                fseek(fp, 14, SEEK_SET);
                fread(&BPB_RsvdSecCnt, 1, 2, fp);

                fseek(fp, 16, SEEK_SET);
                fread(&BPB_NumFATs, 1, 1, fp);

                fseek(fp, 36, SEEK_SET);
                fread(&BPB_FATSz32, 1, 4, fp);
                
                fseek(fp, 0x100400, SEEK_SET);
                fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
                fileOpened = 1;
            }

        }

        if (strcmp("close", token[0]) == 0)
        {
            
            if (fp == NULL)
            {             
              printf("Error: File system not open.\n");
              
            }
    
            int i = fclose(fp);
            fp = NULL;
            printf("File successfully closed!!\n");
            fileOpened = 0;
            
           

        }

        if (strcmp("info", token[0]) == 0)
        {
            if (fileOpened == 1)
            {
                printf("BPB_BytesPerSec : %d\n", BPB_BytesPerSec);
                printf("BPB_BytesPerSec : %x\n", BPB_BytesPerSec);
                printf("\n");

                printf("BPB_SecPerClus : %d\n", BPB_SecPerClus);
                printf("BPB_SecPerClus : %x\n", BPB_SecPerClus);
                printf("\n");

                printf("BPB_RsvdSecCnt : %d\n", BPB_RsvdSecCnt);
                printf("BPB_RsvdSecCnt : %x\n", BPB_RsvdSecCnt);
                printf("\n");

                printf("BPB_NumFATs : %d\n", BPB_NumFATs);
                printf("BPB_NumFATs : %x\n", BPB_NumFATs);
                printf("\n");

                printf("BPB_FATSz32 : %d\n", BPB_FATSz32);
                printf("BPB_FATSz32 : %x\n", BPB_FATSz32);
                printf("\n");
            }
            else
            {
                printf("Error: file not opened\n");

            }

            continue;

        }

        if (strcmp("stat", token[0]) == 0)
        {
            int i=0;
            
            if (fileOpened == 1)
            {
               
                printf("File size: %d\n", dir[i].DIR_FileSize);
                printf("First Cluster Low: %d\n", dir[i].DIR_FirstClusterLow);
                printf("DIR_ATTR: %d\n", dir[i].DIR_Attr);
                printf("First Cluster High: %d\n", dir[i].DIR_FirstClusterHigh);
            }
            else
            {
                printf("Error: file not opened\n");
            }
        }

        if (strcmp("get", token[0]) == 0)
        {
            int i;
            if (fileOpened == 1)
            {
                for (i=0; i<16; i++)
                {

                    int16_t cluster = dir[i].DIR_FirstClusterLow;
                    int16_t size = dir[i].DIR_FileSize;
                    int offset = LBAToOffset(cluster);
                    
                    fseek(fp,offset,SEEK_SET);
                    FILE* ofp = fopen(token[1],"w");
                    uint8_t buffer[512];
                    
                    while(size >= BPB_BytesPerSec)
                    {

                        fread(buffer,512,1,fp);
                        fwrite(buffer,512,1,ofp);
                        size = size - BPB_BytesPerSec;
                        //new offset
                        cluster = NextLB(cluster);
                        offset = LBAToOffset(cluster);
                        fseek(fp,offset,SEEK_SET);
                    }
                    if(size > 0)
                    {
                        fread(buffer,size,1,fp);
                        fwrite(buffer,size,1,ofp);
                    }
                    fclose(ofp);
                }
                
            }
            else
            {
                printf("Error: file not opened\n");
            }

        }

        if (strcmp("cd", token[0]) == 0)
        {
            int i;
            if (fileOpened == 1)
            {
                
                for (i=0; i<16; i++)
                {
                    
                    if(compare(dir[i].DIR_Name, token[1]) == 0)
                    {
                        int cluster = dir[i].DIR_FirstClusterLow;
                        int offset = LBAToOffset(cluster);
                        fseek(fp,offset, SEEK_SET);
                        fread(&dir[0], sizeof(struct DirectoryEntry),16,fp);
                        break;
                    }
                    
                }
            }
            else
            {
                printf("Error: file not opened\n");
            }

        }

        if (strcmp("ls", token[0]) == 0)
        {
            int i;
            if (fileOpened == 1)
            {
                for (i=0; i<16; i++)
                {
                    if ((dir[i].DIR_Attr == 0x01 ||
                        dir[i].DIR_Attr == 0x10 ||
                         dir[i].DIR_Attr == 0x20 ||
                        dir[i].DIR_Attr == 0x30) &&
                        dir[i].DIR_Name[0] != (signed char)0xe5)
                    {
                        char name[12];
                        memcpy(name,dir[i].DIR_Name,11);
                        name[11] = '\0';
                        printf("%s\n", name);
                    }
                }
            }
            else
            {
                printf("Error: file not opened\n");
            }

        }

        if (strcmp("read", token[0]) == 0)
        {
            int i=0;
            if (fileOpened == 1)
            {
                if ((token[2] == NULL) || (token[3] == NULL) || (token[1] == NULL))
                {
                    printf("Error: provide more information\n");
                }
                else
                {
                    int16_t cluster = dir[i].DIR_FirstClusterLow;
                    int16_t size = dir[i].DIR_FileSize;
                    int offset = LBAToOffset(cluster);
                    int locStart;
                    int readSize;
                    
                    locStart = atoi(token[2]);
                    readSize = atoi(token[3]);
                    fseek(fp,offset,SEEK_SET);
                    char buffer[512];
                    fseek(fp,locStart,SEEK_CUR);
                    fread(buffer,readSize,1,fp);
                    for (i=0; i<readSize; i++)
                    {
                        printf("%c", buffer[i]);
                    }
                }
                 
            }
            else
            {
                printf("Error: file not opened\n");
            }
           

        }

        if (strcmp("del", token[0]) == 0)
        {
            int i;
            if (fileOpened == 1)
            {
                for(i=0;i<16;i++)
                {
                    
                    if(compare(dir[i].DIR_Name,token[1]) == 0)
                    {
                        attrPlaceHolder = dir[i].DIR_Attr;
                        dir[i].DIR_Attr = 0xe5;
                        strncpy(placeHolder,dir[i].DIR_Name,11);
                        strcpy(dir[i].DIR_Name, "?");

                        
                    }
                }
      
            }
            else
            {
                printf("Error: file not opened\n");
            }
           
        }

        if (strcmp("undel", token[0]) == 0)
        {
            int i;
            if (fileOpened == 1)
            {
                for(i=0;i<16;i++)
                {
                    if(strcmp("?",dir[i].DIR_Name) == 0)
                    {
                        strncpy(dir[i].DIR_Name, placeHolder, 11);
                        dir[i].DIR_Attr = attrPlaceHolder;

                    }
                }
    
            }
            else
            {
                printf("Error: file not opened\n");
            }
           

        }
        
        if ((strcmp("exit", token[0]) == 0) || (strcmp("quit", token[0])) == 0)
        {
            exit(0);
        }
        





    }
    return 0;
}




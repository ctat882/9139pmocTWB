/***********************************************************************
************************************************************************
***                                                                  ***
***                     9319 ASSIGNMENT 02                           ***
***                                                                  ***
************************************************************************            
***                                                                  ***
***   Filename:  bwtsearch.c                                         ***
***   Author:    Corey Tattam / 3221450                              ***
***   Date:      04/14                                               ***
***   Purpose:   Backwards search on a BWT file and/or decode BWT    *** 
***                                                                  ***   
************************************************************************
***********************************************************************/


/*********************************
 **          #INCLUDES          **
 *********************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bwt.h"


/*********************************
 **          #DEFINES           **
 *********************************/

#define MAX_CHARS 256
#define EOF_BYTE_OFFSET 4



/*/**********************************/
/* **        TYPE DEFINES         ***/
/* *********************************/




/*********************************
 **      FUNCTION PROTOTYPES    **
 *********************************/

static table read_last_char_pos (char *filename);
static void handle_cmd_ln_args (int argc, char *argv[]);
static void create_index_file(char *idx_file_loc,unsigned int bwt_file_size);

/*********************************
 **        DEBUG PROTOTYPES     **
 *********************************/
 
/*********************************
 **        GLOBAL VARIABLES     **
 *********************************/
FILE *bwt_file;
FILE *index_file;
int has_index;
int search_mode;
int idx_file_size;



/**********************************
 **            MAIN              **
 **********************************/
int main (int argc, char *argv[]) 
{
   
   handle_cmd_ln_args(argc,argv);
   /*
      If no index exists then must create one
      -> can we do it without an index file?
   */
   table st = read_last_char_pos(argv[BWT_ARG]);
   // next step is to create an index file (storing Occ/Rank) if one does not
   // exist yet.
   if (! has_index) {
      create_index_file(argv[INDEX_ARG],st->bwt_file_size);
      index_file = fopen(argv[INDEX_ARG],"r");
      fseek(index_file,0,SEEK_END); // get length of index file
      idx_file_size = ftell(index_file);
      rewind(index_file);

/*      //get occ table*/
/*      unsigned int rank[idx_file_size];*/
/*      fread(rank,4,idx_file_size,index_file);*/

/*      int i;*/
/*      for (i = 0; i < idx_file_size; i += 1) {*/
/*         printf("position %d has rank %d\n",i,rank[i]); */
/*      } */

/*      fclose(index_file);*/
       
   }
   // if search mode
   if (search_mode) {
      char *query = (argv[QUERY_ARG]);
      backwards_search(query,st,bwt_file,index_file);
   }
   //TODO Else unbwt
   
   // print c table
/*      print_c_table(st->ctable);*/
/*      print_stats(st);*/
   fclose(bwt_file);
   fclose(index_file);
      
   return 0;
}


 /**********************************
 **      FUNCTION DEFINITIONS     **
 **********************************/

static void handle_cmd_ln_args (int argc, char *argv[]) {
   if (argc == UNBWT_MODE) {
      search_mode = FALSE;
   }
   else if (argc == SEARCH_MODE) {
      search_mode = TRUE;
   }
   else {
      exit(-1);
   }
   bwt_file = fopen(argv[BWT_ARG],"r");
   if (bwt_file == NULL) exit(-1);
   index_file = fopen(argv[INDEX_ARG],"r");
   if (index_file == NULL) {
      has_index = FALSE;
   }
   else {
      has_index = TRUE;
   }
   
} 


static table read_last_char_pos (char *filename) {
   printf("START TO READ FILE\n");
   // open file in read mode
   FILE *file = fopen(filename,"r");
   if (file == NULL) {
      exit(-1);      
   }
   else {
/*      int value;*/
/*      fread(&value,sizeof(value),1,file);*/
/*      printf("value = %d\n",value);*/
      fseek(file,0,SEEK_END);
      unsigned int length = ftell(file);

      // Find the length of the file in bytes
      printf("length of the file is: %d\n",length);
      length -= 4;      
      rewind(file);
      // Get the first 4 bytes of the file to find the location of the 
      // end of BWT character
      unsigned char bytes[4];
      fread(bytes,1,4,file);
      // Convert first four bytes into integer 
      // TODO: Might have to add +1 to this number.
      unsigned int last = *(unsigned int*)bytes;
      printf("Last= %d\n",last);

      // get frequencies;
      int c;
      unsigned int count[MAX_CHARS] = {0};
      while ((c = fgetc(file)) != EOF) {
         count[c]++;
      }

      table st = new_symbol_table();
      
      // create Count table[] from frequencies
      // TODO must free ctable after use
      unsigned int *ctable = create_c_table(count);
      init_symbol_table(st,ctable,count[NEW_LINE_CHAR],length,last);
      
      
      //fclose(file);
      return st;
   }
 
}

static void create_index_file(char *idx_file_loc,unsigned int bwt_file_size) {
   FILE *idx = fopen(idx_file_loc,"w+"); // w+ is for read and write

   unsigned int rank[INDEX_LIMIT] = {0};
   unsigned int char_count[256] = {0};
   // TODO check that offset is the right place to start?? could be +/- 1
   fseek(bwt_file,EOF_BYTE_OFFSET,SEEK_SET);
   int index_size = INDEX_LIMIT;
   if (bwt_file_size < INDEX_LIMIT) index_size = bwt_file_size;
   unsigned char bytes[index_size];
   fread(bytes,1,index_size ,bwt_file);

   int i;
   int c;
   for (i = 0; i < index_size; i += 1) {
      // get char
      c = bytes[i];
      // increment character count
      char_count[c]++;
      // get the rank of that character
      rank[i] = char_count[c];
      
   }
   fwrite(rank,sizeof(int),index_size,idx);
   fclose (idx);
   return;

}



/*********************************
 **       DEBUG DEFINITIONS     **
 *********************************/








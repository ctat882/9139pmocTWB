
#define FALSE 0
#define TRUE 1
#define MAX_CHARS 256
#define EOF_BYTE_OFFSET 4
#define INDEX_LIMIT 512
#define BWT_ARG 1
#define INDEX_ARG 2
#define QUERY_ARG 3
//TODO unbwt mode and search mode may have to be incremented by one/two
// so we can use the out file for bwt
#define UNBWT_MODE 3
#define SEARCH_MODE 4
#define NEW_LINE_CHAR 10

/*********************************
 **        TYPE DEFINES         **
 *********************************/

typedef struct _symbol_table *table;

struct _symbol_table {

   unsigned int *ctable;         // The C[] table
   unsigned int num_lines;       // The number of lines in the table
   unsigned int bwt_file_size;       // The number of bytes in the file (- 1st four bytes)
   unsigned int last;   // Position of the last character in bwt
   unsigned int idx_file_size;
   
} symbol_table;


/*********************************
 **      FUNCTION PROTOTYPES    **
 *********************************/
 
static unsigned int * create_c_table (unsigned int *freq);
static void init_symbol_table (table st, unsigned int *c, unsigned int lines, unsigned int fSize, unsigned int last);
static table new_symbol_table ();
static void backwards_search (char *query,table st, FILE *bwt, FILE *idx);
static int get_last_occurence (unsigned int *ctable, int c);
//static unsigned int occ (int c, int position,FILE *bwt, FILE *idx);
int occ_func(int character,int limit,FILE *bwt);
//static void unbwt(table st);
//static int get_num_lines (table st);

/*********************************
 **        DEBUG PROTOTYPES     **
 *********************************/
//static void print_c_table (unsigned int *ctable);
//static void print_stats (table st);

 /**********************************
 **      FUNCTION DEFINITIONS     **
 **********************************/


/*
   Given a character frequency array, create and return an array
   that stores how many characters are lexicographically less than a given
   character.
   @params: *freq is the character frequency array.
   @return: C[] table (int*)
*/
static unsigned int * create_c_table (unsigned int *freq) {
   int i;   
   unsigned int count = 0;
   unsigned int *c = malloc(sizeof(int)* (MAX_CHARS + 1));
   
   for (i = 1; i < MAX_CHARS; i++ ) {
      if (freq[i] > 0) count += freq[i];
      c[i + 1] = count;
   }
   return c;
}

static void init_symbol_table (table st, unsigned int *c, unsigned int lines, unsigned int fSize, unsigned int last ) {
   st->ctable = c;
   st->num_lines = lines;
   st->bwt_file_size = fSize;
   st->last = last;
}

static table new_symbol_table() {
   table newTable = malloc(sizeof(symbol_table));
   newTable->ctable = NULL;
   newTable->num_lines = 0;
   newTable->bwt_file_size = 0;
   return newTable;
}

static void backwards_search (char *query,table st, FILE *bwt, FILE *idx) {
   // Get First and Last
   short int found_match;
   //initialise variables
   int i = strlen(query) - 1;          // i = |P|
   int c = query[i];                   // 'c' = last character in P
   int first = st->ctable[c] + 1;      // TODO unsure about the + 1
//   int first = 1;
   int last = get_last_occurence (st->ctable,c);
//   int last = st->ctable[MAX_CHARS];
   printf("i = %d, c = %c, First = %d, Last = %d\n",i,c,first,last);
   
   // Run the backwards search algorithm
   /*
   while (First <= Last and i >= 0) {
      c = P[i]
      First = C[c] + Occ(c, First - 1) + 1
      Last = C[c] + Occ(c, Last)
      i = i - 1
   }   
   */
   while ((first <= last) && i >= 1) {
      c = query[i - 1];
      first = st->ctable[c] + occ_func(c,first - 1,bwt) + 1;
      last = st->ctable[c] + occ_func(c,last,bwt);
//      first = st->ctable[c] + occ(c,first - 1,bwt,idx) + 1;
//      last = st->ctable[c] + occ(c,last,bwt,idx);
      i--;
      printf("i = %d, c = %c, First = %d, Last = %d\n",i,c,first,last);
   }
   printf("i = %d, c = %c, First = %d, Last = %d\n",i,c,first,last);
   if ( last < first) { 
      found_match = FALSE;
      //TODO delete this output
      printf("No matches found\n");
   }
   else {
      found_match = TRUE;      
   }
   if(found_match) {
      int matches = last - first + 1;
      printf("Number of matches = %d\n",matches);
   }

   return;
}
//static unsigned int occ (int c, int position,FILE *bwt, FILE *idx) {
//   /*TODO this will have to be modified to handle files that
//      have large index files
//    */
//    unsigned int rank;
//    int c_found = FALSE;
//    fseek(bwt,position - 1,SEEK_SET);
////    unsigned char bwt_byte;
//    //fread(bwt_byte,1,sizeof(unsigned char),bwt);
//    if( fgetc(bwt) == c) {
//      fseek(idx,position - 1,SEEK_SET);
//      unsigned char rank_bytes[4];
//      fread(rank_bytes,1,sizeof(int),idx);
//      rank = *(unsigned int*)rank_bytes;
//    }
//    else {
//      while (! c_found && position > 0) {

//         position = position - 2;
//         fseek(bwt,-position,SEEK_CUR);
//         if (fgetc(bwt) == c) {
//            c_found = TRUE;
//         }
//      }
//      fseek(idx,position - 1,SEEK_SET);
//      unsigned char rank_bytes[4];
//      fread(rank_bytes,1,sizeof(int),idx);
//      rank = *(unsigned int*)rank_bytes;
//    }
//    
//    return rank;

//}

// Occurrence function WITHOUT the use of index file in L Column
int occ_func(int character,int limit,FILE *bwt){
	rewind(bwt);
	fseek(bwt,EOF_BYTE_OFFSET,SEEK_SET);
	int counter = 0;
	int c;
	int x;
	for (x = 1; x <= limit; ++x){
		(c = getc(bwt));
		if (c == character)
			++counter;
	}
	return counter;
}

static int get_last_occurence (unsigned int *ctable, int c) {
   while (ctable[c + 1] == 0) c++;    //TODO not sure about this either
   return ctable[c + 1];
}


 /*********************************
 **       DEBUG DEFINITIONS     **
 *********************************/

//static void print_c_table (unsigned int *ctable) {
//   int i = 0;
//   for(i = 0; i < 127; i++) {
//      printf("%c = %d\n",i,ctable[i]);
//   }
//   return;
//}

//static void print_stats (table st) {
//   printf("NUM LINES = %d\n",st->num_lines);
//   printf("FILE SIZE = %d BYTES\n",st->bwt_file_size);

//}


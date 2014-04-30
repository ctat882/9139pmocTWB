
#define FALSE 0
#define TRUE 1
#define MAX_CHARS 256
#define BWT_OFFSET 4
#define INDEX_LIMIT 512

#define RANK_INTERVAL 2048
#define C_TABLE_OFFSET 1024

// COMMAND LINE ARGUMENTS
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
//static void init_symbol_table (table st, unsigned int *c, unsigned int lines, unsigned int fSize, unsigned int last);
static table new_symbol_table ();
static void backwards_search (char *query,table st, FILE *bwt, FILE *idx);
static int get_last_occurence (unsigned int *ctable, int c);
unsigned int occ (int c, int position,FILE *bwt, FILE *idx);
int occ_func(int character,int limit,FILE *bwt);
int occ_func_pos(int character,int position,FILE *bwt,int from);
//static void unbwt(table st);
//static int get_num_lines (table st);
static unsigned int get_last_char_pos (FILE *bwt);
static unsigned int get_bwt_size (FILE *bwt);
static unsigned int get_idx_size (FILE *idx);
static void c_table_from_idx (table st, FILE *idx);


static void create_idx (char *idx_file_loc, FILE *bwt);

int get_last_char (FILE *bwt,int position);
/*********************************
 **        DEBUG PROTOTYPES     **
 *********************************/
void print_c_table (unsigned int *ctable);
void print_stats (table st);

 /**********************************
 **      FUNCTION DEFINITIONS     **
 **********************************/


static void create_idx (char *idx_file_loc, FILE *bwt) {
   int c;
   unsigned int count[MAX_CHARS] = {0}; //will be used as rank
   unsigned int total_count = 0;        //store the number of characters seen
//   unsigned int rank[MAX_CHARS] = {0};

   // Create new index file
   FILE *idx = fopen(idx_file_loc,"w+");
   fseek(bwt,BWT_OFFSET,SEEK_SET);
//   int debugCount = 0;
   while ((c = fgetc(bwt)) != EOF) {
      count[c]++;
      total_count++;
      /* 
         for every RANK_INTERVAL characters (eg 2048 characters)
         read in, write out the count array to the index file.
         These will be used as starting points to calculate the
         rank.
         This should ensure that the index file does not exceed bwt_size / 2
      */
      if (total_count % RANK_INTERVAL == 0) {
//         debugCount++;
//         printf("total char count = %d, interval: %d\n",total_count++,debugCount);
//         print_c_table(count);
         fwrite (count,sizeof(int),MAX_CHARS,idx);
      }
   }
   // Create C[] table and store at end of index file
   unsigned int *ctable = create_c_table(count);
   fwrite (ctable,sizeof(int),MAX_CHARS,idx);
   printf("C_TABLE:\n");
   print_c_table(ctable);
   
   free (ctable);

   fclose(idx);
}

static void c_table_from_idx (table st, FILE *idx) {
   st->ctable = malloc(sizeof(int) * MAX_CHARS);
   fseek(idx,-C_TABLE_OFFSET,SEEK_END);
   fread(st->ctable,sizeof(int),MAX_CHARS,idx);
}

static unsigned int get_last_char_pos (FILE *bwt) {
   rewind(bwt);
   // Get the first 4 bytes of the file to find the location of the 
   // end of BWT character
   unsigned char bytes[4];
   fread(bytes,1,4,bwt);
   // Convert first four bytes into integer 
   // TODO: Might have to add +1 to this number.
   unsigned int last = *(unsigned int*)bytes;
   return last;
}

int get_last_char (FILE *bwt,int position) {   
   fseek(bwt,position + BWT_OFFSET,SEEK_SET);
   int last_ch = getc(bwt);
   return last_ch;
}


static unsigned int get_bwt_size (FILE *bwt) {
   unsigned int size;
   fseek(bwt,0,SEEK_END);
   size = ftell(bwt) - BWT_OFFSET;
   return size;
}

static unsigned int get_idx_size (FILE *idx) {
   unsigned int size;
   fseek(idx,0,SEEK_END);
   size = ftell(idx);
   return size;
}

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

//static void init_symbol_table (table st, unsigned int *c, unsigned int lines, unsigned int fSize, unsigned int last ) {
//   st->ctable = c;
//   st->num_lines = lines;
//   st->bwt_file_size = fSize;
//   st->last = last;
//}

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
   int first = st->ctable[c] + 1;
   int last = get_last_occurence (st->ctable,c);
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
      if (st->idx_file_size < RANK_INTERVAL) {
         first = st->ctable[c] + occ_func(c,first - 1,bwt) + 1;
         last = st->ctable[c] + occ_func(c,last,bwt);
      }
      else {
         first = st->ctable[c] + occ(c,first - 1,bwt,idx) + 1;
         last = st->ctable[c] + occ(c,last,bwt,idx);
      }
      i--;
      printf("i = %d, c = %c, First = %d, Last = %d\n",i,c,first,last);
   }
   if ( last < first) { 
      found_match = FALSE;
      //TODO delete this output
      printf("No matches found\n");
   }
   else {
      found_match = TRUE;      
   }
   if(found_match) {
      //TODO delete output
      int matches = last - first + 1;
      printf("Number of matches = %d\n",matches);

/*
   NOW TRY AND RECOVER STRING
*/
      printf("Position of last char in bwt = %d\n",st->last);
//      fseek(bwt,st->last,SEEK_SET);
//      int last_ch = getc(bwt);
//      rewind(bwt);
//      c = 0;
//      int occurL;
//      
//      for (i = first; i <= last; i++) {
//         int temp = i - 1;
//         while ( c != last_ch) {
//            fseek(bwt,temp,SEEK_SET);
//            occurL = ftell(bwt);
//            c = getc(bwt);
//            if(c != '\n') {
//               printf("%c",c);
//            }
//            temp = st->ctable[c] + occ(c,occurL,bwt,idx);
//         }
//      }

      
   }

   
   return;
}

unsigned int occ (int c, int position,FILE *bwt,FILE *idx) {
   //TODO check if needed:
   rewind (bwt);
   rewind (idx);

   int rank;
   // If rank is smaller than interval, don't use index
   if (position <= RANK_INTERVAL) {
      rank = occ_func(c,position,bwt);
   }
   else {
      int idx_offset;
      // determine index offset
      if (position >= RANK_INTERVAL && position < (RANK_INTERVAL * 2)) {
         idx_offset = 0;
      }
      else {
         idx_offset = ((position - RANK_INTERVAL)/RANK_INTERVAL) * MAX_CHARS * sizeof(int);
      }
      // get index block 
      fseek(idx,idx_offset,SEEK_SET);
      int * index = (int*) malloc (sizeof(int) * MAX_CHARS);
      fread(index,sizeof(int),MAX_CHARS,idx);
      // get the rank of character c from index block
      unsigned int idx_rank = index[c];
      // determine where to start counting from in the bwt file
      int bwt_start = ((position / RANK_INTERVAL) * RANK_INTERVAL);
      // go to the start position (add 4 byte offset)
      fseek(bwt,(bwt_start + BWT_OFFSET),SEEK_SET);
      int count = 0;
      int i,ch;
      // start bwt count from starting position to given position
      for ( i = bwt_start; i < position; i++ ) {
         ch = getc(bwt);
         if (ch == c) count++; 
      }

//      printf("character: %c, position: %d, idx_int: %d, offset: %d, idx_rank: %d\n", c, position, interval,idx_offset,idx_rank);   
//      printf("occ_func rank is: %d, bwt_offset: %d, count: %d\n",occ_func(c,position,bwt),bwt_start,count); 
      rank = idx_rank + count;
      free(index);
   }
   return rank;

}


// Occurrence function WITHOUT the use of index file in L Column
int occ_func(int character,int position,FILE *bwt){
//	rewind(bwt);
   // Start at beginning of BWT section (add BWT OFFSET)
   fseek(bwt,BWT_OFFSET,SEEK_SET);
	int rank= 0;
	int c;
	int i;
	for (i = 1; i <= position; i++){
		(c = getc(bwt));
		if (c == character) rank++;
	}
	return rank;
}


// Occurrence function WITHOUT the use of index file in L Column
int occ_func_pos(int character,int position,FILE *bwt,int from){
//	rewind(bwt);
   // Start at beginning of BWT section (add BWT OFFSET)
   fseek(bwt,BWT_OFFSET + from,SEEK_SET);
	int rank= 0;
	int c;
	int i;
	for (i = from; i < position; i++){
		(c = getc(bwt));
		if (c == character) rank++;
	}
	return rank;
}

static int get_last_occurence (unsigned int *ctable, int c) {
   while (ctable[c + 1] == 0) c++;    //TODO not sure about this either
   return ctable[c + 1];
}


 /*********************************
 **       DEBUG DEFINITIONS     **
 *********************************/

void print_c_table (unsigned int *ctable) {
   int i = 0;
   for(i = 0; i < 127; i++) {
      printf("%c = %d\n",i,ctable[i]);
   }
   return;
}

void print_stats (table st) {
//   printf("NUM LINES = %d\n",st->num_lines);
//   printf("FILE SIZE = %d BYTES\n",st->bwt_file_size);

   printf("SIZE of BWT file is %d\n",st->bwt_file_size);
   printf("SIZE of index file is %d\n",st->idx_file_size);

}


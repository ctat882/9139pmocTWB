
#define FALSE 0
#define TRUE 1
#define MAX_CHARS 256
#define BWT_OFFSET 4
#define INDEX_LIMIT 512
#define MAX_STRING_LEN 200

#define RANK_INTERVAL 2048
#define C_TABLE_OFFSET 1024

// COMMAND LINE ARGUMENTS
#define BWT_ARG 1
#define INDEX_ARG 2
#define QUERY_ARG 3
#define UNBWT_ARG 4
//TODO unbwt mode and search mode may have to be incremented by one/two
// so we can use the out file for bwt
#define UNBWT_MODE 5
#define SEARCH_MODE 4
#define NEW_LINE_CHAR 10

#define FIRST 0
#define LAST 1

/*********************************
 **        TYPE DEFINES         **
 *********************************/

/*
   Result object used in search and string reconstruction
*/ 
typedef struct _result_object *result;
struct _result_object {
   unsigned int id;        //identify the line (use the last char or '\n')
   char *b_string;  //backwards search results string
   char *f_string;   //forwards search string
   short int b_length;     //length of backwards result string
   short int f_length;  //length of forward result string
   result next;            //the next result;  
   
} result_object;

/*
   Symbol table used to hold C array and other statitistics
*/
typedef struct _symbol_table *table;
struct _symbol_table {

   unsigned int *ctable;         // The C[] table
   unsigned int num_lines;       // The number of lines in the table
   unsigned int bwt_size;        // # bytes in the file (- 1st four bytes)
   unsigned int last;            // Position of the last character in bwt
   unsigned int idx_size;        // # bytes in index
   
   
} symbol_table;


/*********************************
 **      FUNCTION PROTOTYPES    **
 *********************************/
 
/* STRUCTS */
static table new_symbol_table ();
result new_result ();

/* SEARCH RELATED FUNCTIONS */
static void backwards_search (char *query,table st, FILE *bwt, FILE *idx);
static void  get_first_and_last (char *query,table st, FILE *bwt, FILE *idx, int *fnl);
result backwards_results (int *fnl,table st, FILE *bwt, FILE *idx);
void forward_results(int *fnl,result head,table st, FILE *bwt, FILE *idx);
int pos_of_rank_c_in_bwt (int c,int rank,table st, FILE *bwt, FILE *idx);
void search_for_duplicate_lines (result head);
void sort_b_strings(result head);
void free_results(result head);
int count_results (result head);
/* UNIVERSAL */
static int get_last_occurence (unsigned int *ctable, int c);
unsigned int occ (int c, int position,FILE *bwt, FILE *idx);
int occ_func(int character,int limit,FILE *bwt);
static unsigned int get_last_char_pos (FILE *bwt);
static unsigned int get_bwt_size (FILE *bwt);
static unsigned int get_idx_size (FILE *idx);
int get_last_char (FILE *bwt,int position);
static void c_table_from_idx (table st, FILE *idx);

/* INDEX CREATION FUNCTIONS */
static void create_idx (char *idx_file_loc, FILE *bwt);
static unsigned int * create_c_table (unsigned int *freq);



/*********************************
 **        DEBUG PROTOTYPES     **
 *********************************/
void print_c_table (unsigned int *ctable);
void print_stats (table st);

 /**********************************
 **      FUNCTION DEFINITIONS     **
 **********************************/


result new_result () {
   result r = malloc(sizeof(result_object));   
   r->id = 0;
   r->b_string = malloc(sizeof(char) * MAX_STRING_LEN);   
   memset(r->b_string,0,sizeof(char) * MAX_STRING_LEN);     // set everything to 0
   r->f_string = malloc(sizeof(char) * MAX_STRING_LEN);  
   memset(r->f_string,0,sizeof(char) * MAX_STRING_LEN);     // set everything to 0
   r->b_length = 0;
   r->f_length = 0;
   r->next = NULL;
   return r;
}


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
//   printf("C_TABLE:\n");
//   print_c_table(ctable);
   
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


static table new_symbol_table() {
   table newTable = malloc(sizeof(symbol_table));
   newTable->ctable = NULL;
   newTable->num_lines = 0;
   newTable->bwt_size = 0;
   return newTable;
}

void backwards_search (char *query,table st, FILE *bwt, FILE *idx) {
   int fnl[2]; // First and Last values
   get_first_and_last (query,st,bwt,idx,fnl);
   // determine results
   if ( fnl[LAST] < fnl[FIRST]) {
      //TODO delete this output
      printf("No matches found\n");
   }
   else {      
      //TODO delete output
      int matches = fnl[LAST] - fnl[FIRST] + 1;
      printf("Number of matches = %d\n",matches);
      /*
         NOW RECOVER STRING
      */
      result head = backwards_results(fnl,st,bwt,idx); 
      // delete duplicate lines     
       
      forward_results(fnl,head,st,bwt,idx);
      
      search_for_duplicate_lines(head);

      sort_b_strings(head);

//      result r = head;

//      while (r != NULL) {
//         char sorted[r->b_length];
//         int i;
//         int j = 0;
//         for (i = r->b_length - 1; i >= 0; i--) {
//            sorted[j] = r->b_string[i];
//            j++;
//         }
//         for(j = 0; j < r->b_length; j++) {
//            printf("%c",sorted[j]);
//         }
//         for(j = 0; j < r->f_length; j++) {
//            printf("%c",r->f_string[j]);
//         }
//         printf("\n");
//         r = r->next;
//         
//      }
      
      
      // test:
      result t = head;
      while (t != NULL) {
         int j;
         for(j = 0; j < t->b_length; j++) {
            printf("%c",t->b_string[j]);
         }
//         printf(" %s",query);
         for(j = 0; j < t->f_length; j++) {
            printf("%c",t->f_string[j]);
         }
         printf("\n");
         t = t->next;
      }

      free_results(head);      
   }   
   
   
   
   return;
}

int count_results (result head) {
   int count = 0;
   result r = head;
   while (r != NULL) {
      count++;
      r = r->next;
   }
   return count;
}

void free_results (result head) {
   result cur = head;
   while (cur != NULL) {
      result temp = cur;
      cur = cur->next;
      if(temp->b_string != NULL) free(temp->b_string);
      if(temp->f_string != NULL) free(temp->f_string);
      if(temp != NULL) free(temp); 
      temp = NULL;     
      
   }
   
   head = NULL;
}

void sort_b_strings(result head) {
   result r = head;

   while (r != NULL) {
      char *sorted = malloc(sizeof(char) * r->b_length);
      int i;
      int j = 0;
      for (i = r->b_length - 1; i >= 0; i--) {
         sorted[j] = r->b_string[i];
         j++;
      }
      char * temp =  r->b_string;
      r->b_string = sorted;
      free(temp);
      r = r->next;
      
   }
}


void search_for_duplicate_lines (result head){
   result cur = head;
//         result last = head;
   result next = NULL;
   int id;
   while (cur != NULL) {
      id = cur->id;
      next = cur->next;
      result before = cur;
      while(next != NULL) {
         if (next->id == id) {
            result temp = next;
            next = next->next;
            before->next = next;
            if(temp->b_string != NULL) free(temp->b_string);
            if(temp->f_string != NULL) free(temp->f_string);
            if(temp != NULL) free(temp); 
//            free(temp->b_string);
//            free(temp->f_string);
//            free(temp);
         }
         else {
            before = next;
            next = next->next;
         }
      }
//            last = cur;
      cur = cur->next;
   }
}


void forward_results(int *fnl,result head,table st, FILE *bwt, FILE *idx) {
   result cur = head;
   int i;
   int c = 0;
   fseek(bwt,st->last + BWT_OFFSET,SEEK_SET);
   int last_ch = getc(bwt);
//   rewind(bwt);
   int result_count = 0;

   
   // Iterate through the results
   // MUST KEEP first - 1
   for (i = fnl[FIRST] - 1; i < fnl[LAST]; i++) {
      // create result, update links
      int str_len = 0;
      int pos = i;
      int j;
      int f_occ;
      // get the character in F at pos
      for(j = 0; j < MAX_CHARS; j++) {
         if(st->ctable[j] <= pos) {
            c = j;
         }
      }
      // store c
      while ( c != last_ch && c != '\n') {
         cur->f_string[str_len] = c;
         str_len++;
         if(str_len == (MAX_STRING_LEN -1)) printf("###########ERROR STRLEN AT MAX FORWARDS)");
         //TODO delete this output
//         printf("%c",c);      
         // get the next position   
         f_occ = pos - st->ctable[c] + 1;
         // get the bwt position of character c with rank = f_occ 
         pos = pos_of_rank_c_in_bwt (c,f_occ,st,bwt,idx);
         // get character in f at pos
         for(j = 0; j < MAX_CHARS; j++) {
            if(st->ctable[j] <= pos) {
               c = j;
            }
         }               
      }
      result_count++;
      cur->f_length = str_len;   
      cur = cur->next;
   }

}
// TODO IMPLEMENT BINARY SEARCH HERE
int pos_of_rank_c_in_bwt (int c,int rank,table st, FILE *bwt, FILE *idx) {
   // keep track of position in bwt
   int pos = -1;  // initialise at -1 because of 0 base
   int ch;
   int idx_offset = 0;
   int count = 0;
   // Determine if the index file is needed
   if (st->bwt_size < RANK_INTERVAL) {
      // Go to the beginning of BWT file
      fseek(bwt,BWT_OFFSET,SEEK_SET);
      while (count != rank) {
         pos++;
         ch = getc(bwt);
         if (ch == c) count++;
      }
   }
   // Otherwise use index file
   else {
      int *index = (int*) malloc (sizeof(int) * MAX_CHARS);
      rewind(idx);
      fread(index,sizeof(int),MAX_CHARS,idx);
      
      if (index[c] >= rank) {
         // Have to count from beginning of BWT file
         fseek(bwt,BWT_OFFSET,SEEK_SET);
         while (count != rank) {
            pos++;
            ch = getc(bwt);
            if (ch == c) count++;
         }
      }
      // not in the first interval
      else {
         int interval;  // first index interval
         int max_interval = (st->bwt_size / RANK_INTERVAL);
         int gone_past = FALSE;
         for (interval = 1; interval < max_interval; interval++) {
            idx_offset = (((interval * RANK_INTERVAL) - RANK_INTERVAL) / RANK_INTERVAL) * MAX_CHARS * sizeof(int);
            fseek(idx,idx_offset,SEEK_SET);
            fread(index,sizeof(int),MAX_CHARS,idx);
            if (index[c] >= rank) {
               gone_past = TRUE;
               break;
            }
            
         }
         if (gone_past) {
            interval--;
            idx_offset = (((interval * RANK_INTERVAL) - RANK_INTERVAL) / RANK_INTERVAL) * MAX_CHARS * sizeof(int);
//            idx_offset = interval * MAX_CHARS * sizeof(int);
            fseek(idx,idx_offset,SEEK_SET);
            fread(index,sizeof(int),MAX_CHARS,idx);
            
            pos = interval * RANK_INTERVAL;
            fseek(bwt,BWT_OFFSET + pos,SEEK_SET);
            count = index[c];
            while (count != rank) {
               pos++;
               ch = getc(bwt);
               if (ch == c) count++;
            }
            
         }
         // Must be inbetween the end of the last index block  
         // and the end of the BWT. - WORST CASE
         else {
//            printf("####DEBUG pos_of_rank: START LAST AREA\n");
//            idx_offset = (max_interval - 1) * MAX_CHARS * sizeof(int);
idx_offset = ((((max_interval - 1) * RANK_INTERVAL) - RANK_INTERVAL) / RANK_INTERVAL) * MAX_CHARS * sizeof(int);
            fseek(idx,idx_offset,SEEK_SET);
            fread(index,sizeof(int),MAX_CHARS,idx);
            pos = (max_interval - 1) * RANK_INTERVAL;
            fseek(bwt,BWT_OFFSET + pos,SEEK_SET);
            count = index[c];
            while (count != rank) {
               pos++;
               ch = getc(bwt);
               if (ch == c) count++;
            }
         }

      }

      free(index);
//      fseek(bwt,BWT_OFFSET,SEEK_SET);
//      int debugcount = 0;
//      int debugpos = -1;
//      while (debugcount != rank) {
//         debugpos++;
//         ch = getc(bwt);
//         if (ch == c) debugcount++;
//      }
//      printf("DEBUGPOS = %d, POS = %d\n",debugpos,pos);
      pos--;
   }

   return pos;
}


result backwards_results (int *fnl,table st, FILE *bwt, FILE *idx){
   result head = NULL;
   result last = NULL;
   int i;
   int c = 0;
   fseek(bwt,st->last + BWT_OFFSET,SEEK_SET);
   int last_ch = getc(bwt);
   rewind(bwt);
   int result_count = 0;

   /*
      BACKWARDS
   */
   
   // Iterate through the results
   // MUST KEEP first - 1
   for (i = fnl[FIRST] - 1; i < fnl[LAST]; i++) {
      // create result, update links
      result r = new_result();
      if(result_count == 0 ) {
         head = r;         
      }
      else {
         last->next = r;
      }
      int str_len = 0;
      int pos = i;
      fseek(bwt,pos + BWT_OFFSET,SEEK_SET);
      c = getc(bwt);
      // Get the string
      while ( c != last_ch && c != '\n') {
         r->b_string[str_len] = c;
         str_len++;
         if(str_len == (MAX_STRING_LEN - 1)) printf("###########ERROR STRLEN AT MAX BACKWARDS)");
         //TODO delete this output
//         printf("%c",c);      
         // get the next position      
         pos = st->ctable[c] + occ(c,pos,bwt,idx);
         fseek(bwt,pos + BWT_OFFSET,SEEK_SET);
         // get character
         c = getc(bwt);               
      }
      // set r->id to '\n' position in bwt
      r->id = ftell(bwt) - 1;
      r->b_length = str_len;
//         printf("%s",query);
      //TODO delete this output
//      if (c == '\n') {
//         printf("%c",c);
//      }
      result_count++;
      // update link
      last = r;
   }
   return head;
}


static void  get_first_and_last (char *query,table st, FILE *bwt, FILE *idx, int *fnl) {
   // Get First and Last
//   short int found_match;
   //initialise variables
   int i = strlen(query) - 1;          // i = |P|
   int c = query[i];                   // 'c' = last character in P
   int first = st->ctable[c] + 1;
   int last = get_last_occurence (st->ctable,c);
//   printf("i = %d, c = %c, First = %d, Last = %d\n",i,c,first,last);
   
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
      if (st->idx_size < RANK_INTERVAL) {
         first = st->ctable[c] + occ_func(c,first - 1,bwt) + 1;
         last = st->ctable[c] + occ_func(c,last,bwt);
      }
      else {
         first = st->ctable[c] + occ(c,first - 1,bwt,idx) + 1;
         last = st->ctable[c] + occ(c,last,bwt,idx);
      }
      i--;
//      printf("i = %d, c = %c, First = %d, Last = %d\n",i,c,first,last);
   }
   fnl[FIRST] = first;
   fnl[LAST] = last;

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
//   printf("FILE SIZE = %d BYTES\n",st->bwt_size);

   printf("SIZE of BWT file is %d\n",st->bwt_size);
   printf("SIZE of index file is %d\n",st->idx_size);

}


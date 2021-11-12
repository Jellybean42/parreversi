#define mailbox_c

#include "mailbox.h"

#define NO_MAILBOXES 30

static void *shared_memory = NULL;
static mailbox *freelist = NULL;  /* list of free mailboxes.  */


/*
 *  initialise the data structures of mailbox.  Assign prev to the
 *  mailbox prev field.
 */

static mailbox *mailbox_config (mailbox *mbox, mailbox *prev)
{
  mbox->data.result = 0;
  mbox->data.move_no = 0;
  mbox->data.positions_explored = 0;
  mbox->prev = prev;
  mbox->item_available = multiprocessor_initSem (0);
  mbox->space_available = multiprocessor_initSem (1);
  mbox->mutex = multiprocessor_initSem (1);
  return mbox;
}


/*
 *  init_memory - initialise the shared memory region once.
 *                It also initialises all mailboxes.
 */

static void init_memory (void)
{
  if (shared_memory == NULL)
    {
      mailbox *mbox;
      mailbox *prev = NULL;
      int i;
      _M2_multiprocessor_init ();
      shared_memory = multiprocessor_initSharedMemory
	(NO_MAILBOXES * sizeof (mailbox));
      mbox = shared_memory;
      for (i = 0; i < NO_MAILBOXES; i++)
	prev = mailbox_config (&mbox[i], prev);
      freelist = prev;
    }
}


/*
 *  init - create a single mailbox which can contain a single triple.
 */

mailbox *mailbox_init (void)
{
  mailbox *mbox;

  init_memory ();
  if (freelist == NULL)
    {
      printf ("exhausted mailboxes\n");
      exit (1);
    }
  mbox = freelist;
  freelist = freelist->prev;
  return mbox;
}


/*
 *  kill - return the mailbox to the freelist.  No process must use this
 *         mailbox.
 */

mailbox *mailbox_kill (mailbox *mbox)
{
  mbox->prev = freelist;
  freelist = mbox;
  return NULL;
}


/*
 *  send - send (result, move_no, positions_explored) to the mailbox mbox.
 */
void mailbox_send (mailbox *mbox, int result, int move_no, int positions_explored)
{
  const int BUFFER_SIZE=3;

  key_t key;
  int shmid;

  if((key = ftok(".", "S"))==-1) //return key value from file location and ID value
  {                              //"." in linux refers to the current position, S is the ID value
    printf("Unable to make key");//if the key is reurned as -1 an error has occured
    fflush(stdout);              //sometimes printfs were getting lost so flushing to stdout ensures the user is notified of any errors
  }

  shmid = shmget(key, (BUFFER_SIZE + 1) * sizeof(int), 0644 | IPC_CREAT); //create shared memory, giving the key, size of the memory segment relative the buffer size 
                                                                          //and access permissions or instruction to create if it doesn't exist
  if(shmid == -1)   //if shmid returns -1 then shared memory couldn't be created or accessed
  {                 //e.g. if no key was made
    printf("Unable to get sh space");     //notify user of error
    fflush(stdout);                       //ensure notification is flushed to stdout
    exit(1);                              //kill the process
  }

  int *data = (int *)shmat(shmid, (void *)0, 0);  //cast data as int so it can store integers, then attach memory

  int *counter = data; //set first element to the counter
  *counter = 0;        //set the counter to 0

  int *buffer = data + 1; //set second element to the data
  int produced = 0; //clear produced
  int pos = 0;  //set pos to start of buffer

  //printf("C: %d    BS: %d \n", *counter, BUFFER_SIZE);
  //fflush(stdout);

  while (*counter >= BUFFER_SIZE){}; //do nothing and wait if the buffer is full

  //printf("Send - while loop \n");
  //fflush(stdout);

  buffer[pos] =  result;            //send result to start of the buffer
  pos = (pos + 1) % BUFFER_SIZE;    //increment the buffer position wrapped to the buffer size

  buffer[pos] =  move_no;           //send the move number to the middle of the buffer
  pos = (pos + 1) % BUFFER_SIZE;    //increment the buffer position wrapped to the buffer size

  buffer[pos] =  positions_explored;//send the number of explored positions to the end of the buffer
  pos = (pos + 1) % BUFFER_SIZE;    //increment the buffer position wrapped to the buffer size

  *counter = BUFFER_SIZE;           //set the counter to show the buffer is full

  //printf("Send Called! result: %d moveN: %d PosEx: %d \n", result, move_no, positions_explored);
  //fflush(stdout);

  //printf("send: %d \n", produced - 1);
  //fflush(stdout);

  shmdt(data);      //detach process from shared memory
  exit(0);          //end the process, it would exit in paro64bit if it returned but I just killed it here
}



/*
 *  rec - receive (result, move_no, positions_explored) from the
 *        mailbox mbox.
 */

void mailbox_rec (mailbox *mbox, int *result, int *move_no, int *positions_explored)
{
  const int BUFFER_SIZE=3;

  key_t key;
  int shmid;

  if((key = ftok(".", "S"))==-1)  //return key from file location and ID
  {
    printf("Unable to make key"); //if -1 was returned no key was returned so notify the user of the error
    fflush(stdout); //flush to console to ensure user recieves the error
  }

  shmid = shmget(key, (BUFFER_SIZE + 1) * sizeof(int), 0644 | IPC_CREAT); //create shared memory, giving the key, size of the memory segment relative the buffer size 
                                                                          //and access permissions or instruction to create if it doesn't exist
  if(shmid == -1) //if shmid returns -1 then shared memory couldn't be created or accessed
  {
    printf("Unable to get sh space"); //notify user of the error
    fflush(stdout); //flush to ensure user recieves error
    exit(1);  //kill process since shared memory can't be accessed
  }

  int *data = (int *)shmat(shmid, (void *)0, 0);  //cast data as int so it can store integers, then attach memory

  int *counter = data;  //set first element to the counter
  int *buffer = data + 1; //set second element to the data

  int produced = 0;
  int pos = 0;

  while(1)
  {
    while (*counter == 0){}; //if the buffer is empty wait until it's not

    *result = buffer[pos];  //get result from start of buffer
    pos = (pos + 1) % BUFFER_SIZE; //increment position on buffer
    *move_no = buffer[pos]; //get move number from middle of buffer
    pos = (pos + 1) % BUFFER_SIZE;  //increment position on buffer
    *positions_explored = buffer[pos];  //get number of explored positions from end of buffer
    pos = (pos + 1) % BUFFER_SIZE;  //increment position on buffer

    *counter = 0; //set counter to show empty buffer

    //printf("recieved! result: %d moveN: %d PosEx: %d \n", *result, *move_no, *positions_explored);
    //fflush(stdout);

    shmdt(data); //detatch from shared memory
    return(0); //return from while loop
  }
  return(0); //return from function back to paro64bit
}
/* $Header: /home/amarin/uc3m/docen/aptel/12-13/practica/RCS/hablamos.c,v 1.3 2012/12/04 12:22:52 amarin Exp amarin $
   $Log: hablamos.c,v $
   Revision 1.3  2012/12/04 12:22:52  amarin
   Fixed search, tsearch and creategroup to use binary form

   Revision 1.2  2012/10/23 09:25:12  amarin
   contacts added

   Revision 1.1  2012/10/16 13:47:27  amarin
   Initial revision

   Autor: Andrés Marín López -- Universidad Carlos III de Madrid amarin@it.uc3m.es
   Description: Cliente de chat protocolo Hablamos
   GNU GPL: http://www.gnu.org/copyleft/
*/

#include "hablamos.h"
#include <fcntl.h>

USER user;                  /* actual user */
USER contacts[MAXCONTACTS]; /* user's contacts */
int nContacts; /* number of actual contacts */
int misocket= -1;
char *host;
int port;
int deregistered;
int successPending;

void parse_execute_command(char *buf, int size, int got);

void usage(char* prg){
  fprintf(stderr, "Usage: %s -h host -p port [-f command_file]\n", prg);
  exit(-1);
}

void logger(const char *format, ...) {
	va_list	args;

	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
}

int errexit(const char *format, ...) {
	va_list	args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(1);
}

int
connectsock(const char *host, const u_short port)
/*
 * Arguments:
 *      host   - name of host to which connection is desired
 *      port   - desired port
 */
{
	struct hostent	*phe;	/* pointer to host information entry	*/
	struct sockaddr_in sin;	/* an Internet endpoint address		*/
	int	s, type;	/* socket descriptor and socket type	*/

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if ( phe = gethostbyname(host) )
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
		errexit("can't get \"%s\" host entry\n", host);

    /* Allocate a socket */
	if ((s = socket(PF_INET, SOCK_STREAM, 0))==-1)
	  errexit("can't create socket: %s\n", strerror(errno));

    /* Connect the socket */
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		errexit("can't connect to %s.%d: %s\n", 
                        host, port, strerror(errno));
	return s;
}


void writesocket(char *buf, CMD _cmd, unsigned int uid, int size){
  char *aux;
  int sent, count, c, len;
  unsigned int nuid, nsize, ltuid;
  char *cmd;
  unsigned int msguuid; int remaining;
  char *saveptr, *token;
  char* tuid;
  cmd= lcmd[_cmd];
  nuid= htonl(uid);
  sent= 0;
  switch (_cmd){
  case SEND:  /* SEND */
    remaining= get_alfa_uid(buf, &msguuid, size);      
    nsize= htonl(remaining+2*sizeof(unsigned int)); /* uiddst+msguuid+html_snippet */
    /*memcpy((void*)&msguuid,buf,sizeof(unsigned int));*/
    len=strlen(cmd)+4*sizeof(unsigned int)+remaining; /* cmd+uidorg+size+uiddst+msguuid+html_snippet */
    aux= malloc(len);
    memcpy(aux, cmd, strlen(cmd)); count= strlen(cmd);
    memcpy(aux+count, (void*)&nuid, sizeof(unsigned int));  count+=sizeof(unsigned int);
    memcpy(aux+count, (void*)&nsize, sizeof(unsigned int)); count+=sizeof(unsigned int);
    nuid= htonl(user.uid);
    memcpy(aux+count, (void*)&nuid, sizeof(unsigned int));  count+=sizeof(unsigned int);
    msguuid= htonl(msguuid);
    memcpy(aux+count, (void*)&msguuid, sizeof(unsigned int));  count+=sizeof(unsigned int);
    memcpy(aux+count, buf+size-remaining, remaining); count+=remaining; 
    if (count>len)
      errexit("error in memory allocation in %s cmd, alloc'd for %d, used %d", cmd, len, count);
    break;
  case CREATEGROUP:
    nsize= htonl(size);
    aux=malloc(11+4+size);
    memcpy(aux, cmd, 11); count=11;
    memcpy(aux+count,(void*)&nsize, sizeof(unsigned int)); count+=sizeof(unsigned int);
    tuid= strtok_r(buf+12,"-",&saveptr);
    memcpy(aux+count, tuid, strlen(tuid));count+=strlen(tuid);
    memcpy(aux+count, "-", 1); count++; 
    size-=12+strlen(tuid); 
    for (token= strtok_r(NULL," \n",&saveptr), size-=strlen(token); token!=NULL && size>0; token= strtok_r(NULL," ",&saveptr), size-=strlen(token)){ 
      nuid= htonl(atoi(token)); 
      memcpy(aux+count, (void*)&nuid, sizeof(unsigned int));  
      count+=sizeof(unsigned int); 
    }
    /* recalculate nsize */
    nsize= htonl(count-11-4);
    memcpy(aux+11,(void*)&nsize, sizeof(unsigned int));
    break;
  case TSEARCH:
    nsize= size-1+sizeof(unsigned int);
    aux= malloc(nsize);
    nsize-=strlen(cmd)+sizeof(unsigned int);
    nsize= htonl(nsize);
    memcpy(aux, cmd, strlen(cmd)); count= strlen(cmd);
    memcpy(aux+count, (void*)&nsize, sizeof(unsigned int));  count+=sizeof(unsigned int);
    memcpy(aux+count, buf+strlen(cmd)+1, size-strlen(cmd)-1);
    count= size-1+sizeof(unsigned int);
    break;
  case REGISTER:
    aux= malloc(sizeof(WD_USER)+strlen(cmd));
    memcpy(aux, cmd, strlen(cmd)); 
    count= strlen(cmd)+htonuser(aux+strlen(cmd),&user);
    break;
  case SEARCH:
    /*    aux= malloc(strlen(cmd)+2*sizeof(unsigned int)); */
    aux= malloc(strlen(cmd)+sizeof(unsigned int)); 
    memcpy(aux, cmd, strlen(cmd)); count= strlen(cmd);
    /*    nsize= htonl(sizeof(unsigned int)); 
	  memcpy(aux+count, (void*)&nsize, sizeof(unsigned int));  count+=sizeof(unsigned int); */
    memcpy(aux+count, (void*)&nuid, sizeof(unsigned int));  count+=sizeof(unsigned int);
    break;
  default: /* DELETEGROUP */
    len= strlen(cmd)+2*sizeof(unsigned int); /* cmd+size+uid */
    aux= malloc(len);
    nsize= htonl(sizeof(unsigned int));
    memcpy(aux, cmd, strlen(cmd)); count= strlen(cmd);
    memcpy(aux+count, (void*)&nsize, sizeof(unsigned int)); count+=sizeof(unsigned int); 
    memcpy(aux+count, (void*)&nuid, sizeof(unsigned int)); count+=sizeof(unsigned int);
    break;
  }
  /*  printf("sending STATUS= %s, (%d bytes)\n",buf,count); */
  
  for (sent=0; sent < count; sent+=c)
    c= write(misocket, &aux[sent], count-sent);
  if (aux!=NULL){
    free(aux); aux= NULL;
  }
}

void readresponse(char *resp, int cnt){
  int inchars, header, remaining;
  unsigned int size, msguuid;
  char *long_resp;
  if (!strncasecmp(resp,"SUCCESS",7)){
    if (cnt>10){
      memcpy(&size,resp+7,sizeof(unsigned int));    
      size= ntohl(size);
      switch (size){
      case 0:
	logger ("Received success (size=0)\n");successPending--;
	break;
      case 4:
	memcpy(&msguuid,resp+11,sizeof(unsigned int));
	msguuid= ntohl(msguuid);
	logger ("Received success msguuid=%d\n",msguuid);successPending--;
	break;
      default:
	successPending--;
	long_resp= malloc(size+2);
	memcpy(long_resp,resp+11,cnt-11);
	if (cnt<size+11){
	  for (inchars = 0; inchars < size-cnt; inchars+=cnt ) {
	    cnt= read(misocket, &long_resp[cnt+inchars], size+11-cnt);
	    if (cnt < 0)
	      errexit("socket read failed: %s\n",strerror(errno));
	  }
	}
	while ((ntohuser((char*)&contacts[nContacts], &long_resp))!=NULL){
	  logger("received user %s\n", usertostr(NULL,&contacts[nContacts]));
	  nContacts++;
	}
	if (long_resp!=NULL){
	  free(long_resp);long_resp= NULL;
	}
	break;
      }
      if (cnt>11+size)
	parse_execute_command(resp+11+size, cnt-11-size, 1);
    }
    else
      logger("Received SUCCESS\n");
  }
  else
    logger("Received unexpected %s\n",resp);
}

void do_creategroup(CMD cmd, unsigned int uid, char *buf, int size){
  if (misocket==-1)
    misocket= connectsock(host,port);
  writesocket(buf, cmd, uid, size);
  successPending++;
}

void do_deletegroup(CMD cmd, unsigned int uid, int size){
  if (misocket==-1)
    misocket= connectsock(host,port);
  writesocket("", cmd, uid, size);
  successPending++;
}

void do_register(CMD cmd, unsigned int uid, char *buf, int size){
  char resp[LINELEN];
  struct sockaddr_in dir;
  socklen_t dirl;
  dirl= sizeof(struct sockaddr_in);
  if (misocket==-1)
    misocket= connectsock(host,port);
  if (getpeername(misocket, (struct sockaddr*)&dir, &dirl)!=0)
    create_user(&user, uid, buf, host, port);
  else
    create_user(&user, uid, buf, inet_ntoa(dir.sin_addr), ntohs(dir.sin_port));
  writesocket("", cmd, user.uid, 2);
  /*  readresponse(resp, cmd); */
}

void do_search(CMD cmd, unsigned int uid, int size){
  if (misocket==-1)
    misocket= connectsock(host,port);
  writesocket("", cmd, uid, size);
  successPending++;
}

void do_dereg(CMD cmd){
  char resp[LINELEN];
  int c;
  if (misocket==-1)
    misocket= connectsock(host,port);
  logger("About to deregister the client...");
  c= write(misocket, "DEREGISTER",10);
  /*readresponse(resp, cmd);*/
  deregistered= 1; successPending++;
  logger("Client deregistered, number of SUCCESS pending to receive= %d\n",successPending);
  printf("Teclea q<ENTER> para salir\n");
}

void do_send(CMD cmd, unsigned int uid, char *buf, int size){
  if (misocket==-1)
    misocket= connectsock(host,port);
  writesocket(buf, cmd, uid, size);
  successPending++;
}

void got_delivered(CMD cmd, char *buf, int cnt){
  char *aux;
  unsigned int msguuid, size;
  aux= malloc(7+8);
  memcpy(aux,"SUCCESS",7);
  size= htonl(4);
  memcpy(aux+7,(char *)&size,4);
  memcpy((char*)&msguuid,buf+9,4);
  memcpy(aux+11,(char *)&msguuid,4);
  msguuid= ntohl(msguuid); /* check if message was delivered by me... */
  logger("got %s msguuid=%d\n", lcmd[cmd], msguuid);
  size= write(misocket,aux,15);
  if (cnt>13)
    parse_execute_command(buf+13, cnt-13, 1);

}

void got_send(CMD cmd, char *buf, int cnt){
  char resp[LINELEN];
  char *long_resp;
  unsigned int size, uidorg, uiddst, msguuid, size2;
  int remaining;
  int inchars;
  char *aux;
  memcpy((char *)&size,buf+4,sizeof(unsigned int));
  size= ntohl(size);
  long_resp= malloc(size+1);
  memcpy(long_resp,buf+8,cnt-8);
  if (cnt<size){
    for (inchars = 0; inchars < size-cnt; inchars+=cnt ) {
      cnt= read(misocket, &long_resp[cnt+inchars], size-cnt);
      if (cnt < 0)
	errexit("socket read failed: (trying to get %d bytes) %s\n",size, strerror(errno));
    }
  }
  aux= malloc(cnt+7+4);
  memcpy(aux,"SUCCESS",7);
  size2= htonl(4);
  memcpy(aux+7,(char *)&size2,4);
  memcpy((char*)&uidorg,long_resp,4);
  uidorg= ntohl(uidorg);
  memcpy((char*)&msguuid,long_resp+4,4);
  memcpy(aux+11,(char *)&msguuid,4);
  long_resp[size]='\0';
  msguuid= ntohl(msguuid); /* check if message was delivered by me... */
  logger("got SEND (uid=%d msguuid=%d size=%d) mesg='%s'\nSENT SUCCESS (msguuid=%d)\n", uidorg, msguuid, size, long_resp+8, msguuid);
  size2= write(misocket,aux,15);
  if (long_resp!=NULL){
    free(long_resp);
    long_resp= NULL;
  }
  if (cnt>12+size)
    parse_execute_command(buf+12+size, cnt-12-size, 1);
}

void do_tsearch(CMD cmd, unsigned int uid,char *buf, int size){
  if (misocket==-1)
    misocket= connectsock(host,port);
  writesocket(buf, cmd, uid, size);
  successPending++;
}

int get_alfa_uid(const char *buf, unsigned int *uid, int size){
  char num[20];
  int i,j;
  i=  findex(buf,' ',size); /* for(i=0;buf[i]!=' '; i++); skip command */
  i+= nfindex(&buf[i],' ',size-i); /*for(;buf[i]==' '; i++); skip whitespace */
  for (j=0; buf[i]!=' '; j++, i++)
    num[j]= buf[i];
  num[j]='\0';
  *uid= (unsigned int)atoi(num);
  i+= nfindex(&buf[i],' ',size-i);/* for(;buf[i]==' ';i++); skip whitespace */
  return size-i+1;
}

int get_bin_uid(const char *buf, unsigned int *uid, int size){
  int i,j;
  i=  findex(buf,' ',size); /* for(i=0;buf[i]!=' '; i++); skip command */
  i= nfindex(&buf[i],' ',size); /*for(;buf[i]==' '; i++); skip whitespace */
  memcpy(uid,&buf[i],sizeof(unsigned int));
  *uid= ntohl(*uid);
  i= nfindex(&buf[i+sizeof(unsigned int)],' ',size); /* for (i+=sizeof(unsigned int); buf[i]==' '; i++);  skip whitespace */
  return i;
}

void parse_execute_command(char *buf, int size, int got){
  int body= 1;
  unsigned int uid;
  CMD command;
  INFO info;
  int remaining;
  int sent, rcvd;
  char resp[LINELEN];
  switch (buf[0]){
  case 'C':
  case 'c': 
    command= CREATEGROUP; 
    do_creategroup(command, user.uid, buf, size);
    break;
  case 'D':
  case 'd': 
    if ((buf[4]=='T') || buf[4]=='t'){
      command= DELETEGROUP; 
      remaining= get_alfa_uid(buf, &uid, size);
      do_deletegroup(command, uid, size);
    }
    else
      if ((buf[2]=='R') || buf[2]=='r'){
	command= DEREG;
	do_dereg(command);
      }
      else {
	command= DELIVERED;
	if (got)
	  got_delivered(command, buf, size);
      }
    break;
  case 'E':
  case 'e':
    printf("Received ERR command, size(%d), skipping...\n",size);
    break;
  case 'L':
  case 'l':
    /*list contacts*/
    for (uid=0; uid<nContacts; uid++)
      printf("contacts[%d]=%s\n",uid,usertostr(NULL, &contacts[uid]));
    break;
  case 'M':
  case 'm': /* sending raw command, allows sending malformed messages */
    for (remaining= size; remaining >0; remaining-=sent, buf+= sent)
      sent= write(misocket, buf, remaining);
    /* receive answer */
    rcvd= read(misocket, resp, LINELEN-1);
    printf("%s read %s response (%d bytes) to malformed message",strncasecmp(resp,"ERROR",5)?"NOK":"OK",resp, rcvd);
    break;
  case 'R':
  case 'r': 
    command= REGISTER; 
    remaining= get_alfa_uid(buf, &uid, size); 
    do_register(command, uid, buf+size-remaining, remaining);
    break;
  case 'T':
  case 't':
    command= TSEARCH; 
    do_tsearch(command, user.uid, buf, size);
    break;
  case 'S':
  case 's': 
    if ((buf[2]=='A') || buf[2]=='a'){ /* search */
      command= SEARCH; 
      remaining= get_alfa_uid(buf, &uid, size);
      do_search(command, uid, size);
    }
    else{
      if (!strncasecmp(buf,lcmd[SEND],4)){
	command= SEND; 
	if (got)                 /* incoming message */
	  got_send(command, buf+4, size-4);
	else{
	  remaining= get_alfa_uid(buf, &uid, size); 
	  do_send(command, uid, buf+size-remaining, remaining);
	}
      }
      else
	if ((buf[1]=='U') || (buf[1]=='u')){/* SUCCESS */
	  readresponse(buf,size);
	}
    }
    break;
  case 'Q':
  case 'q': 
    errexit("exiting client");
  default:
    errexit("Unrecognized command: %s",buf);
  }
}

void get_messages_nonblocking(){
  long res;
  int cnt;
  char buf[LINELEN];
  if (misocket!=-1){
    if ((res= fcntl(misocket, F_GETFL))==-1)
      errexit("get_messages_nonblocking, could not get flags %s\n", strerror(errno));
    if ((fcntl(misocket, F_SETFL, res|O_NONBLOCK))==-1)
      errexit("get_messages_nonblocking, could not set O_NONBLOCK flag %s\n", strerror(errno));
    if ((cnt= read(misocket,buf,LINELEN))<0){
      if (errno!=EAGAIN)
	errexit("get_messages_nonblocking, problems with socket: %s\n", strerror(errno));
      if ((fcntl(misocket, F_SETFL, res))<0)
	errexit("get_messages_nonblocking, could not unset O_NONBLOCK flag %s\n", strerror(errno));
      return;
    }
    else{
      parse_execute_command(buf, cnt, 1);
      if ((fcntl(misocket, F_SETFL, res ^ O_NONBLOCK))<0)
	errexit("get_messages_nonblocking, could not unset O_NONBLOCK flag %s\n", strerror(errno));
    }
  }
}

int inputAvailable()  
{
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(0, &fds);
  select(1, &fds, NULL, NULL, &tv);
  return (FD_ISSET(0, &fds));
}

int check_quit(){
  char i;
  if (inputAvailable()){
    i= getchar();
    if ((i=='q') || (i=='Q'))
      return 1;
  }
  return 0;
}

int main(int argc, char **argv){
  char *command_file=NULL;
  int flags= 0, cnt, from;
  long res;
  int opt, interactive= 1;
  int fdcommand;
  FILE *fprefs, *fcontacts;
  char buf[LINELEN+1];
  int readinput;
  int msguuidAct;
  flags = 0;
  while ((opt = getopt(argc, argv, "h:p:f:")) != -1) {
    switch (opt) {
    case 'h':
      host= optarg;
      flags++;
      break;
    case 'p':
      port = atoi(optarg);
      flags++;
      break;
    case 'f':
      command_file= optarg;
      interactive= 0;
      break;
    default: /* '?' */
      usage(argv[0]);
    }
  }
  
  if (flags<2)
    usage(argv[0]);
  logger("starting client for host: %s, port: %d, %s\n", host, port, (interactive)?"interactive":command_file); 
  
  if (!interactive){
    if ((fdcommand= open(command_file, O_RDONLY|O_NONBLOCK))==-1)
      errexit("Could not open command file: %s", command_file);
  }
  else
    fdcommand= 0; /* stdin */
  
  if ((res= (long) fcntl(fdcommand, F_GETFL))==-1)
      errexit("could not get flags %s\n", strerror(errno));
  if ((fcntl(fdcommand, F_SETFL, flags|O_NONBLOCK))==-1)
    errexit("could not set O_NONBLOCK flag %s\n", strerror(errno));


  /* retrieve user uid from preferences file */
  if ((fprefs= fopen("prefs","r"))==NULL){
    logger("Could not open user preference file");
    create_user(&user, 0, "", host, port);
  }
  else{
    read_user(fprefs, &user);
    fclose(fprefs);
  }

  /* retrieve contacts from contacts file */
  if ((fcontacts= fopen("contacts","r"))==NULL)
    logger("Could not open contacts file");
  else{
    cnt= 0; nContacts=0;
    while ((cnt=read_user(fcontacts, &contacts[nContacts]))>0)
      nContacts++;
    fclose(fcontacts);
  }
  cnt= 0; size= 0; deregistered= 0; readinput=1; successPending= 0;
  while (1){
    if (!deregistered && readinput){
      if ((cnt= read(fdcommand, &buf[size], LINELEN-size))==0){
	if (fdcommand!=0){
	  readinput= 0;
	  printf("Teclea q<ENTER> para salir\n");
	}
      }
      else{
	size+= cnt;
	from= 0;
	while ((cnt= findex(&buf[from],'\n',size))>4){
	  parse_execute_command(&buf[from], cnt++, 0);
	  get_messages_nonblocking();
	  from+=cnt;
	  size-=cnt;
	}
	size=0; cnt=0;
      }
    }
    if (deregistered && check_quit())
      break;
    get_messages_nonblocking();
  }
	    
  if ((fprefs= fopen("prefs","w"))==NULL)
    errexit("Could not create preference file");
  if (write_user(fprefs, &user)<0)
    logger("Could not write user to preference file\n");

  if ((fcontacts= fopen("contacts","w"))==NULL)
    errexit("Could not create contacts file");

  for (cnt= 0; cnt< nContacts; cnt++)
    if ((write_user(fcontacts, &contacts[cnt]))<=0)
    logger("Could not write contact %s to contacts file\n", usertostr(NULL,&contacts[cnt]));

  logger("client finished\n");

  return 0;
}


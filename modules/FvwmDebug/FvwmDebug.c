/* This module, and the entire FvwmDebug program, and the concept for
 * interfacing this module to the Window Manager, are all original work
 * by Robert Nation
 *
 * Copyright 1994, Robert Nation. No guarantees or warantees or anything
 * are provided or implied in any way whatsoever. Use this program at your
 * own risk. Permission to use this program for any purpose is given,
 * as long as the copyright is kept intact. */

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include "libs/Module.h"

#include "FvwmDebug.h"

char *MyName;
int fd_width;
int fd[2];

/*
** spawn_xtee - code to execute xtee from a running executable &
** redirect stdout & stderr to it.  Currently sends both to same xtee,
** but you rewrite this to spawn off 2 xtee's - one for each stream.
*/

pid_t spawn_xtee(void)
{
  pid_t pid;
  int PIPE[2];
  char *argarray[256];

  setvbuf(stdout,NULL,_IOLBF,0); /* line buffered */

  if (pipe(PIPE))
  {
    perror("spawn_xtee");
    fprintf(stderr, "ERROR ERRATA -- Failed to create pipe for xtee.\n");
    return 0;
  }

  argarray[0] = "xtee";
  argarray[1] = "-nostdout";
  argarray[2] = NULL;

  if (!(pid = fork())) /* child */
  {
    dup2(PIPE[0], STDIN_FILENO);
    close(PIPE[0]);
    close(PIPE[1]);
    execvp("xtee",argarray);
    exit(1); /* shouldn't get here... */
  }
  else /* parent */
  {
    if (ReapChildrenPid(pid) != pid)
    {
      dup2(PIPE[1], STDOUT_FILENO);
      dup2(PIPE[1], STDERR_FILENO);
      close(PIPE[0]);
      close(PIPE[1]);
    }
  }

  return pid;
} /* spawn_xtee */

/***********************************************************************
 *
 *  Procedure:
 *	main - start of module
 *
 ***********************************************************************/
int main(int argc, char **argv)
{
  const char *temp, *s;

  /* Save our program  name - for error messages */
  temp = argv[0];
  s=strrchr(argv[0], '/');
  if (s != NULL)
    temp = s + 1;

  MyName = safemalloc(strlen(temp)+2);
  strcpy(MyName, "*");
  strcat(MyName, temp);

  if((argc != 6)&&(argc != 7))
    {
      fprintf(stderr,"%s Version %s should only be executed by fvwm!\n",MyName,
	      VERSION);
      exit(1);
    }

  /* Dead pipe == Fvwm died */
  signal(SIGPIPE, DeadPipe);

  fd[0] = atoi(argv[1]);
  fd[1] = atoi(argv[2]);

#if 0
  spawn_xtee();
#endif

  /* Data passed in command line */
  fprintf(stderr,"Application Window 0x%s\n",argv[4]);
  fprintf(stderr,"Application Context %s\n",argv[5]);

  fd_width = GetFdWidth();

  /* Create a list of all windows */
  /* Request a list of all windows,
   * wait for ConfigureWindow packets */
  SendInfo(fd,"Send_WindowList",0);

  /* tell fvwm we're running */
  SendFinishedStartupNotification(fd);

  Loop(fd);
  return 0;
}


/***********************************************************************
 *
 *  Procedure:
 *	Loop - wait for data to process
 *
 ***********************************************************************/
void Loop(const int *fd)
{
    while (1) {
      FvwmPacket* packet = ReadFvwmPacket(fd[1]);
      if ( packet == NULL )
        exit(0);
      process_message( packet->type, packet->body );
    }
}


/***********************************************************************
 *
 *  Procedure:
 *	Process message - examines packet types, and takes appropriate action
 *
 ***********************************************************************/
void process_message(unsigned long type, const unsigned long *body)
{
  switch(type)
    {
    case M_OLD_ADD_WINDOW:
      list_old_add(body);
    case M_OLD_CONFIGURE_WINDOW:
      list_old_configure(body);
      break;
    case M_DESTROY_WINDOW:
      list_destroy(body);
      break;
    case M_FOCUS_CHANGE:
      list_focus(body);
      break;
    case M_NEW_PAGE:
      list_new_page(body);
      break;
    case M_NEW_DESK:
      list_new_desk(body);
      break;
    case M_RAISE_WINDOW:
      list_raise(body);
      break;
    case M_LOWER_WINDOW:
      list_lower(body);
      break;
    case M_ICONIFY:
      list_iconify(body);
      break;
    case M_MAP:
      list_map(body);
      break;
    case M_ICON_LOCATION:
      list_icon_loc(body);
      break;
    case M_DEICONIFY:
      list_deiconify(body);
      break;
    case M_WINDOW_NAME:
      list_window_name(body);
      break;
    case M_ICON_NAME:
      list_icon_name(body);
      break;
    case M_RES_CLASS:
      list_class(body);
      break;
    case M_RES_NAME:
      list_res_name(body);
      break;
    case M_END_WINDOWLIST:
      list_end();
      break;
    case M_ADD_WINDOW:
      list_add(body);
      break;
    case M_CONFIGURE_WINDOW:
      list_configure(body);
      break;
    case M_DEFAULTICON:
    case M_ICON_FILE:
    default:
      list_unknown(body);
      fprintf(stderr, " 0x%x\n", (int)type);
      break;
    }
}



/***********************************************************************
 *
 *  Procedure:
 *	SIGPIPE handler - SIGPIPE means fvwm is dying
 *
 ***********************************************************************/
void DeadPipe(int nonsense)
{
  (void)nonsense;
  fprintf(stderr,"FvwmDebug: DeadPipe\n");
  exit(0);
}

/***********************************************************************
 *
 *  Procedure:
 *	list_old_add - displays packet contents to stderr
 *
 ***********************************************************************/
void list_old_add(const unsigned long *body)
{
  fprintf(stderr,"Old Add Window\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t frame x %ld\n",(long)body[3]);
  fprintf(stderr,"\t frame y %ld\n",(long)body[4]);
  fprintf(stderr,"\t frame w %ld\n",(long)body[5]);
  fprintf(stderr,"\t frame h %ld\n",(long)body[6]);
  fprintf(stderr,"\t desk %ld\n",(long)body[7]);
  fprintf(stderr,"\t flags %lx\n",body[8]);
  fprintf(stderr,"\t title height %ld\n",(long)body[9]);
  fprintf(stderr,"\t border width %ld\n",(long)body[10]);
  fprintf(stderr,"\t window base width %ld\n",(long)body[11]);
  fprintf(stderr,"\t window base height %ld\n",(long)body[12]);
  fprintf(stderr,"\t window resize width increment %ld\n",(long)body[13]);
  fprintf(stderr,"\t window resize height increment %ld\n",(long)body[14]);
  fprintf(stderr,"\t window min width %ld\n",(long)body[15]);
  fprintf(stderr,"\t window min height %ld\n",(long)body[16]);
  fprintf(stderr,"\t window max %ld\n",(long)body[17]);
  fprintf(stderr,"\t window max %ld\n",(long)body[18]);
  fprintf(stderr,"\t icon label window %lx\n",body[19]);
  fprintf(stderr,"\t icon pixmap window %lx\n",body[20]);
  fprintf(stderr,"\t window gravity %lx\n",body[21]);
}

/***********************************************************************
 *
 *  Procedure:
 *	list_old_configure - displays packet contents to stderr
 *
 ***********************************************************************/
void list_old_configure(const unsigned long *body)
{
  fprintf(stderr,"Old Configure Window\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t frame x %ld\n",(long)body[3]);
  fprintf(stderr,"\t frame y %ld\n",(long)body[4]);
  fprintf(stderr,"\t frame w %ld\n",(long)body[5]);
  fprintf(stderr,"\t frame h %ld\n",(long)body[6]);
  fprintf(stderr,"\t desk %ld\n",(long)body[7]);
  fprintf(stderr,"\t flags %lx\n",body[8]);
  fprintf(stderr,"\t title height %ld\n",(long)body[9]);
  fprintf(stderr,"\t border width %ld\n",(long)body[10]);
  fprintf(stderr,"\t window base width %ld\n",(long)body[11]);
  fprintf(stderr,"\t window base height %ld\n",(long)body[12]);
  fprintf(stderr,"\t window resize width increment %ld\n",(long)body[13]);
  fprintf(stderr,"\t window resize height increment %ld\n",(long)body[14]);
  fprintf(stderr,"\t window min width %ld\n",(long)body[15]);
  fprintf(stderr,"\t window min height %ld\n",(long)body[16]);
  fprintf(stderr,"\t window max %ld\n",(long)body[17]);
  fprintf(stderr,"\t window max %ld\n",(long)body[18]);
  fprintf(stderr,"\t icon label window %lx\n",body[19]);
  fprintf(stderr,"\t icon pixmap window %lx\n",body[20]);
  fprintf(stderr,"\t window gravity %lx\n",body[21]);
}

/***********************************************************************
 *
 *  Procedure:
 *	list_destroy - displays packet contents to stderr
 *
 ***********************************************************************/
void list_destroy(const unsigned long *body)
{
  fprintf(stderr,"destroy\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
}

/***********************************************************************
 *
 *  Procedure:
 *	list_focus - displays packet contents to stderr
 *
 ***********************************************************************/
void list_focus(const unsigned long *body)
{
  fprintf(stderr,"focus\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);

}

/***********************************************************************
 *
 *  Procedure:
 *	list_new_page - displays packet contents to stderr
 *
 ***********************************************************************/
void list_new_page(const unsigned long *body)
{
  fprintf(stderr,"new page\n");
  fprintf(stderr,"\t x %ld\n",(long)body[0]);
  fprintf(stderr,"\t y %ld\n",(long)body[1]);
  fprintf(stderr,"\t desk %ld\n",(long)body[2]);
}

/***********************************************************************
 *
 *  Procedure:
 *	list_new_desk - displays packet contents to stderr
 *
 ***********************************************************************/
void list_new_desk(const unsigned long *body)
{
  fprintf(stderr,"new desk\n");
  fprintf(stderr,"\t desk %ld\n",(long)body[0]);
}

/***********************************************************************
 *
 *  Procedure:
 *	list_raise - displays packet contents to stderr
 *
 ***********************************************************************/
void list_raise(const unsigned long *body)
{
  fprintf(stderr,"raise\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
}


/***********************************************************************
 *
 *  Procedure:
 *	list_lower - displays packet contents to stderr
 *
 ***********************************************************************/
void list_lower(const unsigned long *body)
{
  fprintf(stderr,"lower\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
}


/***********************************************************************
 *
 *  Procedure:
 *	list_unknow - handles an unrecognized packet.
 *
 ***********************************************************************/
void list_unknown(const unsigned long *body)
{
  (void)body;
  fprintf(stderr,"Unknown packet type");
}

/***********************************************************************
 *
 *  Procedure:
 *	list_iconify - displays packet contents to stderr
 *
 ***********************************************************************/
void list_iconify(const unsigned long *body)
{
  fprintf(stderr,"iconify\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t icon x %ld\n",(long)body[3]);
  fprintf(stderr,"\t icon y %ld\n",(long)body[4]);
  fprintf(stderr,"\t icon w %ld\n",(long)body[5]);
  fprintf(stderr,"\t icon h %ld\n",(long)body[6]);
}


/***********************************************************************
 *
 *  Procedure:
 *	list_map - displays packet contents to stderr
 *
 ***********************************************************************/
void list_map(const unsigned long *body)
{
  fprintf(stderr,"map\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
}


/***********************************************************************
 *
 *  Procedure:
 *	list_icon_loc - displays packet contents to stderr
 *
 ***********************************************************************/
void list_icon_loc(const unsigned long *body)
{
  fprintf(stderr,"icon location\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t icon x %ld\n",(long)body[3]);
  fprintf(stderr,"\t icon y %ld\n",(long)body[4]);
  fprintf(stderr,"\t icon w %ld\n",(long)body[5]);
  fprintf(stderr,"\t icon h %ld\n",(long)body[6]);
}



/***********************************************************************
 *
 *  Procedure:
 *	list_deiconify - displays packet contents to stderr
 *
 ***********************************************************************/

void list_deiconify(const unsigned long *body)
{
  fprintf(stderr,"de-iconify\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
}

/***********************************************************************
 *
 *  Procedure:
 *	list_window_name - displays packet contents to stderr
 *
 ***********************************************************************/

void list_window_name(const unsigned long *body)
{
  fprintf(stderr,"window name\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t window name %s\n",(const char *)(&body[3]));

}


/***********************************************************************
 *
 *  Procedure:
 *	list_icon_name - displays packet contents to stderr
 *
 ***********************************************************************/
void list_icon_name(const unsigned long *body)
{
  fprintf(stderr,"icon name\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t icon name %s\n",(const char *)(&body[3]));
}



/***********************************************************************
 *
 *  Procedure:
 *	list_class - displays packet contents to stderr
 *
 ***********************************************************************/
void list_class(const unsigned long *body)
{
  fprintf(stderr,"window class\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t window class %s\n",(const char *)(&body[3]));
}


/***********************************************************************
 *
 *  Procedure:
 *	list_res_name - displays packet contents to stderr
 *
 ***********************************************************************/
void list_res_name(const unsigned long *body)
{
  fprintf(stderr,"class resource name\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t resource name %s\n",(char *)(&body[3]));
}

/***********************************************************************
 *
 *  Procedure:
 *	list_end - displays packet contents to stderr
 *
 ***********************************************************************/
void list_end(void)
{
  fprintf(stderr,"Send_WindowList End\n");
}


/***********************************************************************
 *
 *  Procedure:
 *	list_add - displays packet contents to stderr
 *
 ***********************************************************************/
void list_add(const unsigned long *body)
{
  fprintf(stderr,"Add Window\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t frame x %ld\n",(long)body[3]);
  fprintf(stderr,"\t frame y %ld\n",(long)body[4]);
  fprintf(stderr,"\t frame w %ld\n",(long)body[5]);
  fprintf(stderr,"\t frame h %ld\n",(long)body[6]);
  fprintf(stderr,"\t desk %ld\n",(long)body[7]);
  /*
  fprintf(stderr,"\t flags %lx\n",body[8]);
  */
  fprintf(stderr,"\t title height %ld\n",(long)body[9]);
  fprintf(stderr,"\t border width %ld\n",(long)body[10]);
  fprintf(stderr,"\t window base width %ld\n",(long)body[11]);
  fprintf(stderr,"\t window base height %ld\n",(long)body[12]);
  fprintf(stderr,"\t window resize width increment %ld\n",(long)body[13]);
  fprintf(stderr,"\t window resize height increment %ld\n",(long)body[14]);
  fprintf(stderr,"\t window min width %ld\n",(long)body[15]);
  fprintf(stderr,"\t window min height %ld\n",(long)body[16]);
  fprintf(stderr,"\t window max %ld\n",(long)body[17]);
  fprintf(stderr,"\t window max %ld\n",(long)body[18]);
  fprintf(stderr,"\t icon label window %lx\n",body[19]);
  fprintf(stderr,"\t icon pixmap window %lx\n",body[20]);
  fprintf(stderr,"\t window gravity %lx\n",body[21]);
  fprintf(stderr,"\t forecolor %lx\n",body[22]);
  fprintf(stderr,"\t backcolor %lx\n",body[23]);
  fprintf(stderr,"\t flags %lx\n",body[24]);
}

/***********************************************************************
 *
 *  Procedure:
 *	list_configure - displays packet contents to stderr
 *
 ***********************************************************************/
void list_configure(const unsigned long *body)
{
  fprintf(stderr,"Configure Window\n");
  fprintf(stderr,"\t ID %lx\n",body[0]);
  fprintf(stderr,"\t frame ID %lx\n",body[1]);
  fprintf(stderr,"\t fvwm ptr %lx\n",body[2]);
  fprintf(stderr,"\t frame x %ld\n",(long)body[3]);
  fprintf(stderr,"\t frame y %ld\n",(long)body[4]);
  fprintf(stderr,"\t frame w %ld\n",(long)body[5]);
  fprintf(stderr,"\t frame h %ld\n",(long)body[6]);
  fprintf(stderr,"\t desk %ld\n",(long)body[7]);
  /*
  fprintf(stderr,"\t flags %lx\n",body[8]);
  */
  fprintf(stderr,"\t title height %ld\n",(long)body[9]);
  fprintf(stderr,"\t border width %ld\n",(long)body[10]);
  fprintf(stderr,"\t window base width %ld\n",(long)body[11]);
  fprintf(stderr,"\t window base height %ld\n",(long)body[12]);
  fprintf(stderr,"\t window resize width increment %ld\n",(long)body[13]);
  fprintf(stderr,"\t window resize height increment %ld\n",(long)body[14]);
  fprintf(stderr,"\t window min width %ld\n",(long)body[15]);
  fprintf(stderr,"\t window min height %ld\n",(long)body[16]);
  fprintf(stderr,"\t window max %ld\n",(long)body[17]);
  fprintf(stderr,"\t window max %ld\n",(long)body[18]);
  fprintf(stderr,"\t icon label window %lx\n",body[19]);
  fprintf(stderr,"\t icon pixmap window %lx\n",body[20]);
  fprintf(stderr,"\t window gravity %lx\n",body[21]);
  fprintf(stderr,"\t forecolor %lx\n",body[22]);
  fprintf(stderr,"\t backcolor %lx\n",body[23]);
  fprintf(stderr,"\t flags %lx\n",body[24]);
}

#ifndef __WINE_SERVER_SESSION_H
#define __WINE_SERVER_SESSION_H

struct session
{
    struct object  obj;
    unsigned int  session_id;
	struct list  entry;              /* entry in global session list */
	struct list  processes;         /* list of processes of this session */
	struct list  winstations;        /* list of winstations of this session */
};

extern struct session *get_process_session( struct process *process, unsigned int access );

#endif 

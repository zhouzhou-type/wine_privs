#include "config.h"
#include "wine/port.h"

#include <stdio.h>
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winternl.h"

#include "object.h"
#include "handle.h"
#include "request.h"
#include "process.h"
#include "user.h"
#include "file.h"
#include "security.h"
#include "wine/unicode.h"
#include "wine/debug.h"

#include "session.h"  

static struct list session_list = LIST_INIT(session_list);

static void session_dump( struct object *obj, int verbose );
static struct object_type *session_get_type( struct object *obj );
static unsigned int session_map_access( struct object *obj, unsigned int access );
static int session_close_handle( struct object *obj, struct process *process, obj_handle_t handle );
static void session_destroy( struct object *obj );

static struct object_ops session_ops =
{
    sizeof(struct session),    /* size */
    session_dump,              /* dump */
    session_get_type,          /* get_type */
    no_add_queue,               /* add_queue */
    NULL,                         /* remove_queue */
    NULL,                         /* signaled */
    NULL,                         /* satisfied */
    no_signal,                   /* signal */
    no_get_fd,                  /* get_fd */
    session_map_access,        /* map_access */
    default_get_sd,               /* get_sd */
    default_set_sd,               /* set_sd */
    no_lookup_name,       /* lookup_name */
    /* because session object don't need namespace,so don't need lookup_name */
    directory_link_name,          /* link_name */
    default_unlink_name,          /* unlink_name */
    no_open_file,                 /* open_file */
    session_close_handle,      /* close_handle */
    session_destroy            /* destroy */
};

/* finally */
static void session_dump( struct object *obj, int verbose )
{
	struct session*session=(struct session*)obj;
	fprintf(stderr,"session_id=%d,name=%s",session->session_id,debugstr_w(session->obj.name->name));
}

/* finally */
static struct object_type *session_get_type( struct object *obj )
{	/* lack this function,app WinObj will crash */
	static const WCHAR name[] = {'S','e','s','s','i','o','n'};    
	static const struct unicode_str str = { name, sizeof(name) };    
	return get_object_type( &str );
}

static unsigned int session_map_access( struct object *obj, unsigned int access )
{	/* alloc_handle require this function */
	/* FIXME */
	return 0;
}

static int session_close_handle( struct object *obj, struct process *process, obj_handle_t handle )
{    
	return 0;
}

static void session_destroy( struct object *obj )
{
	struct session *session = (struct session *)obj;
	list_remove(&session->entry);
}

struct session *create_session( struct object *root,const struct unicode_str *name, 
	unsigned int session_id ,unsigned int attr, unsigned int flags)
{
	struct session * session;
	
	if((session = create_named_object( root, &session_ops, name, attr, NULL )))
	{
		if(get_error()!=STATUS_OBJECT_NAME_EXISTS)
		{
			session->session_id = session_id;
			list_add_tail( &session_list, &session->entry );
			list_init(&session->processes);
			list_init(&session->winstations);
		}
		else
			clear_error();
	}

	return session;
}

struct session *get_process_session( struct process *process, unsigned int access )  //lyl
{
	assert( process->session != 0);
    return (struct session *)get_handle_obj( process, process->session,access, &session_ops );
}

DECL_HANDLER(enum_session)
{
	struct session* current_session=get_process_session(current->process,0);
	struct session* id=NULL;
	int i=0;
	struct list* p;

	LIST_FOR_EACH(p, &session_list)
	{
		id = LIST_ENTRY( p, struct session, entry );
		if(!id->session_id)
		{
			reply->ids[i] = id->session_id;
			i++;
		}
		else if(current_session->session_id == id->session_id)
		{
			reply->ids[i] = id->session_id;
			i++;
			break;
		}
	}
	reply->size = i;
	release_object(current_session);
}

DECL_HANDLER(get_process_session)
{
    reply->handle = current->process->session;
}

DECL_HANDLER(set_process_session)
{
    struct session *session;

    if ((session = (struct session *)get_handle_obj( current->process, req->handle,0, &session_ops )))
    {
        /* FIXME: should we close the old one? */
        current->process->session = req->handle;
        release_object( session );
    }
}

DECL_HANDLER(create_session)
{
	struct session *session;    
	unsigned int session_id = req->session_id;    
	struct unicode_str name = get_req_unicode_str();    
	struct object *root = NULL;    
	reply->handle = 0;
	
	if (req->rootdir && !(root = get_directory_obj( current->process, req->rootdir ))) 
		return;  
	
    if((session = create_session(root,&name,session_id,req->attributes,req->flags)))
    {
    	/* Add the current process to the session process list */
    	list_add_tail(&session->processes , &current->process->session_process_links );
		//fprintf(stderr,"add\n");
		
        reply->handle = alloc_handle( current->process, session, req->access, req->attributes );
        release_object( session );
    }

	if (root) 
		release_object( root );
}


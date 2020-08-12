/*
 * Tokens
 *
 * Copyright (C) 1998 Alexandre Julliard
 * Copyright (C) 2003 Mike McCormack
 * Copyright (C) 2005 Robert Shearman
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

#include "handle.h"
#include "thread.h"
#include "process.h"
#include "request.h"
#include "security.h"

#include "token.h"

#include "wine/unicode.h"

#define MAX_SUBAUTH_COUNT 1

const LUID SeIncreaseQuotaPrivilege        = {  5, 0 };
const LUID SeSecurityPrivilege             = {  8, 0 };
const LUID SeTakeOwnershipPrivilege        = {  9, 0 };
const LUID SeLoadDriverPrivilege           = { 10, 0 };
const LUID SeSystemProfilePrivilege        = { 11, 0 };
const LUID SeSystemtimePrivilege           = { 12, 0 };
const LUID SeProfileSingleProcessPrivilege = { 13, 0 };
const LUID SeIncreaseBasePriorityPrivilege = { 14, 0 };
const LUID SeCreatePagefilePrivilege       = { 15, 0 };
const LUID SeBackupPrivilege               = { 17, 0 };
const LUID SeRestorePrivilege              = { 18, 0 };
const LUID SeShutdownPrivilege             = { 19, 0 };
const LUID SeDebugPrivilege                = { 20, 0 };
const LUID SeSystemEnvironmentPrivilege    = { 22, 0 };
const LUID SeChangeNotifyPrivilege         = { 23, 0 };
const LUID SeRemoteShutdownPrivilege       = { 24, 0 };
const LUID SeUndockPrivilege               = { 25, 0 };
const LUID SeManageVolumePrivilege         = { 28, 0 };
const LUID SeImpersonatePrivilege          = { 29, 0 };
const LUID SeCreateGlobalPrivilege         = { 30, 0 };

const SID world_sid = { SID_REVISION, 1, { SECURITY_WORLD_SID_AUTHORITY }, { SECURITY_WORLD_RID } };
const SID local_sid = { SID_REVISION, 1, { SECURITY_LOCAL_SID_AUTHORITY }, { SECURITY_LOCAL_RID } };
const SID interactive_sid = { SID_REVISION, 1, { SECURITY_NT_AUTHORITY }, { SECURITY_INTERACTIVE_RID } };
const SID anonymous_logon_sid = { SID_REVISION, 1, { SECURITY_NT_AUTHORITY }, { SECURITY_ANONYMOUS_LOGON_RID } };
const SID authenticated_user_sid = { SID_REVISION, 1, { SECURITY_NT_AUTHORITY }, { SECURITY_AUTHENTICATED_USER_RID } };
const SID local_system_sid = { SID_REVISION, 1, { SECURITY_NT_AUTHORITY }, { SECURITY_LOCAL_SYSTEM_RID } };

const PSID security_builtin_admins_sid = (PSID)&builtin_admins_sid;
const PSID security_builtin_users_sid = (PSID)&builtin_users_sid;

const PSID security_world_sid = (PSID)&world_sid;
const PSID security_local_sid = (PSID)&local_sid;
const PSID security_interactive_sid = (PSID)&interactive_sid;
const PSID security_authenticated_user_sid = (PSID)&authenticated_user_sid;
const PSID security_local_system_sid = (PSID)&local_system_sid;

struct local_user_sid_def local_user_sid = { SID_REVISION, 5, { SECURITY_NT_AUTHORITY }, { SECURITY_NT_NON_UNIQUE, 0, 0, 0, 1000 } };
struct builtin_admins_sid_def builtin_admins_sid = { SID_REVISION, 2, { SECURITY_NT_AUTHORITY }, { SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS } };
struct builtin_users_sid_def builtin_users_sid = { SID_REVISION, 2, { SECURITY_NT_AUTHORITY }, { SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS } };

const PSID security_local_user_sid = (PSID)&local_user_sid;

luid_t prev_luid_value = { 1000, 0 };

const struct object_ops token_ops =
{
    sizeof(struct token),      /* size */
    token_dump,                /* dump */
    no_get_type,               /* get_type */
    no_add_queue,              /* add_queue */
    NULL,                      /* remove_queue */
    NULL,                      /* signaled */
    NULL,                      /* satisfied */
    no_signal,                 /* signal */
    no_get_fd,                 /* get_fd */
    token_map_access,          /* map_access */
    default_get_sd,            /* get_sd */
    default_set_sd,            /* set_sd */
    no_lookup_name,            /* lookup_name */
    no_link_name,              /* link_name */
    NULL,                      /* unlink_name */
    no_open_file,              /* open_file */
    no_close_handle,           /* close_handle */
    token_destroy              /* destroy */
};



//zyq group management
static struct list group_list = LIST_INIT( group_list );
/*NET_API_STATUS WINAPI NetLocalGroupAdd (group_name)
{
     NET_API_STATUS status;
	 struct group *gp = NULL;
	 SID *sid = mem_alloc(FIELD_OFFSET(SID, SubAuthority[5]));
	 gp = HeapAlloc(GetProcessHeap(), 0, sizeof(struct local_group));
	 if(!gp)
        {
            status = NERR_InternalError;
            break;
        }
	 gp->lgrpi1_comment = NULL;
	 gp->lgrpi1_name = group_name;
	 sid->Revision = SID_REVISION;
//	 sid->SubAuthorityCount = 5;
	 sid->IdentifierAuthority = 5;
	 if(group_name == "spec"){
	 	sid->SubAuthority = {SECURITY_NT_NON_UNIQUE,0,0,0,2000};
		gp->sid = sid;
	 }
	 	 
	 if(group_name == "norm"){
         sid->SubAuthority = {SECURITY_NT_NON_UNIQUE,0,0,0,2001};
		 gp->sid = sid;
	 }
	 list_add_head(&group_list, &gp->entry);
	 status = NERR_Success;
	 return status;
}
*/

struct sidlistnode{
    SID val;
	struct sidlistnode *next;
	struct sidlistnode(){}
	struct sidlistnode(SID valu,sidlist *next1){
        val = valu;
		next = next1;
	}
};

struct luidlistnode{
    LUID val;
	struct luidlistnode *next;
	struct luidlistnode(){}
	struct luidlistnode(SID valu,sidlist *next1){
        val = valu;
		next = next1;
	}
};

void luidlistaddtotail(luidlistnode **phead, LUID val){
    luidlistnode *pnew = new luidlistnode();
	pnew->val = val;
	pnew->next = NULL;
	if(phead == NULL){
        *phead = pnew;
	}
	else{
        luidlistnode *pnode = *phead;
		while(pnode->next != NULL){
            pnode = pnode->next;
		}
		pnode->next = pnew;
	}
}

//zyq sysprivilege management
struct sidlistnode *sechangenotify_sid = new sidlistnode();
struct sidlistnode *sesecurity_sid = new sidlistnode();
struct sidlistnode *sebackup_sid = new sidlistnode();
struct sidlistnode *serestore_sid = new sidlistnode();
struct sidlistnode *sesystemtime_sid = new sidlistnode();
struct sidlistnode *seshutdown_sid = new sidlistnode();
struct sidlistnode *seremoteshutdown_sid = new sidlistnode();
struct sidlistnode *setakeownership_sid = new sidlistnode();
struct sidlistnode *sedebug_sid = new sidlistnode();
struct sidlistnode *sesystemenvironment_sid = new sidlistnode();
struct sidlistnode *sesystemprofile_sid = new sidlistnode();
struct sidlistnode *seprofilesingleprocess_sid = new sidlistnode();
struct sidlistnode *seincreasebasepriority_sid = new sidlistnode();
struct sidlistnode *seloaddriver_sid = new sidlistnode();
struct sidlistnode *secreatepagefile_sid = new sidlistnode();
struct sidlistnode *seincreasequota_sid = new sidlistnode();
struct sidlistnode *seundock_sid = new sidlistnode();
struct sidlistnode *semanagevolume_sid = new sidlistnode();
struct sidlistnode *seimpersonate_sid = new sidlistnode();
struct sidlistnode *secreateglobal_sid = new sidlistnode();



struct privilege sysprivs[] = {
	{ SeChangeNotifyPrivilege		 , 0,0, sechangenotify_sid},
	{ SeSecurityPrivilege			 , 0,0, sesecurity_sid	  },
	{ SeBackupPrivilege 			 , 0,0, sebackup_sid	  },
	{ SeRestorePrivilege			 , 0,0, serestore_sid	  },
	{ SeSystemtimePrivilege 		 , 0,0, sesystemtime_sid  },
	{ SeShutdownPrivilege			 , 0,0, seshutdown_sid	  },
	{ SeRemoteShutdownPrivilege 	 , 0,0, seremoteshutdown_sid				},
	{ SeTakeOwnershipPrivilege		 , 0,0, 	setakeownership_sid				},
	{ SeDebugPrivilege				 , 0,0,		sedebug_sid			},
	{ SeSystemEnvironmentPrivilege	 , 0,0,sesystemenvironment_sid					},
	{ SeSystemProfilePrivilege		 , 0,0,sesystemprofile_sid					},
	{ SeProfileSingleProcessPrivilege, 0,0,		seprofilesingleprocess_sid			},
	{ SeIncreaseBasePriorityPrivilege, 0,0,		seincreasebasepriority_sid			},
	{ SeLoadDriverPrivilege 		 , 0,0,  seloaddriver_sid},
	{ SeCreatePagefilePrivilege 	 , 0,0, 	secreatepagefile_sid				},
	{ SeIncreaseQuotaPrivilege		 , 0,0, seincreasequota_sid					},
	{ SeUndockPrivilege 			 , 0,0,seundock_sid					},
	{ SeManageVolumePrivilege		 , 0,0,semanagevolume_sid					},
	{ SeImpersonatePrivilege		 , 0,0,  seimpersonate_sid},
	{ SeCreateGlobalPrivilege		 , 0,0,  secreateglobal_sid},

};


/*void adjust_sysprivilege_add_group(LUID luid, LPWSTR group_name )
{
	SID *lgsid = mem_alloc(FIELD_OFFSET(SID, SubAuthority[5]));
    struct group *lgp = &group_list;
	while(lgp){
		if(lgp->lgrpi1_name == group_name){
            lgsid = &lgp->sid;
			break;
		}
		lgp = lgp->entry->next;
	}
	//struct list lgsidlist = LIST_INIT(lgsidlist);
	struct privilege *syspriv = &privilege_list;
	while(syspriv){
        if(syspriv->luid == &luid){
            //lgsidlist = syspriv->lgsid;
			list_add_head(&syspriv->lgsid,&lgsid);
			break;
		}
		syspriv = syspriv->entry->next;
	}
	//list_add_head(&lgsidlist, &lgsid);
}
static struct list user_list = LIST_INIT( user_list );
void adjust_sysprivilege_add_user(LUID luid, LPWSTR user_name){
    SID *usid = mem_alloc(FIELD_OFFSET(SID, SubAuthority[5]));
	struct user *us = &user_list;
	while(us){
        if(us->usri1_name == user_name){
           usid = us->sid;
		   break;
		}
		us = us->entry->next;
	}
	struct list usidlist;
	struct privilege *syspriv = &privilege_list;
	while(syspriv){
        if(syspriv->luid == luid){
            usidlist = syspriv->usid;
			break;
		}
		syspriv = syspriv->entry->next;
	}
	list_add_head(&usidlist,&usid);
}
*/

//zyq add group sid by user sid
/*static struct list user_group_list = LIST_INIT( user_group_list );
SID *usid_to_gsid(SID *usid){
	struct user_group_relations *ug = NULL;
	struct group *gp = NULL;
    SID *gsid =  mem_alloc(FIELD_OFFSET(SID, SubAuthority[5]));
    gsid->Revision = usid->Revision;
    gsid->IdentifierAuthority = 2;
	gsid->SubAuthority = usid->SubAuthority;
	gp->sid = gsid;
	list_add_head(&group_list, &gp->entry);
	ug->u_sid = usid;
	ug->g_sid = gsid;
	list_add_head(&user_group_list,&ug->entry);
	return gsid;
}
*/


void token_dump( struct object *obj, int verbose )
{
    fprintf( stderr, "Security token\n" );
    /* FIXME: dump token members */
}

unsigned int token_map_access( struct object *obj, unsigned int access )
{
    if (access & GENERIC_READ)    access |= TOKEN_READ;
    if (access & GENERIC_WRITE)   access |= TOKEN_WRITE;
    if (access & GENERIC_EXECUTE) access |= STANDARD_RIGHTS_EXECUTE;
    if (access & GENERIC_ALL)     access |= TOKEN_ALL_ACCESS;
    return access & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

static const unsigned int alias_admins_subauth[] = { SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS };
static const unsigned int alias_users_subauth[] = { SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS };
/* on Windows, this value changes every time the user logs on */
static const unsigned int logon_subauth[] = { SECURITY_LOGON_IDS_RID, 0, 1 /* FIXME: should be randomly generated when tokens are inherited by new processes */ };

//danqi
//hyy
static SID *alloc_security_sid( const SID_IDENTIFIER_AUTHORITY *authorities,
                         const BYTE subauth_count, const DWORD subauthes[] )
{
    int i;
    SID *sid = mem_alloc( FIELD_OFFSET(SID, SubAuthority[subauth_count]) );
    if (!sid) return NULL;
    sid->Revision = SID_REVISION;
    sid->SubAuthorityCount = subauth_count;
    sid->IdentifierAuthority = *authorities;
    for (i = 0; i < subauth_count; i++)
        sid->SubAuthority[i] = subauthes[i];
    return sid;
}

void security_set_thread_token( struct thread *thread, obj_handle_t handle )
{
    if (!handle) {
        if (thread->token)
            release_object( thread->token );
        thread->token = NULL;
    } else {
        struct token *token = (struct token *)get_handle_obj(
                current->process, handle, TOKEN_IMPERSONATE, &token_ops );
        if (token) {
            if (thread->token)
                release_object( thread->token );
            thread->token = token;
        }
    }
    /*
    if (thread->token) {
        fprintf(stderr, "hyy:TOKEN sid=%u\n", thread->token->user->IdentifierAuthority);
    } else {
        fprintf(stderr, "hyy:TOKEN=null\n");
    }
    */
}

//hyy only used by file_get_sd and dir_get_sd and first_token
//zyq
const SID *security_unix_uid_to_sid( uid_t uid )
{
    SID_IDENTIFIER_AUTHORITY id;
    memset(&id, 0, sizeof(id));
    *(uint32_t *)(&id) = ((uint32_t)uid);
    //id.Value[5] = 1;
    id.Value[5] = 5;

    //BYTE subauth_count = 3;
    BYTE subauth_count = 5;
    /*DWORD subauthes[] = {
        SECURITY_LOGON_IDS_RID,
        0,
        1 /* FIXME: should be randomly generated when tokens are inherited by new processes 
    };*/
    DWORD subauthes[] = {
        SECURITY_NT_NON_UNIQUE,
		0,
		0,
		0,
		uid
	};
    return alloc_security_sid(&id, subauth_count, subauthes);
}
//hyy
//zyq
const SID *security_unix_gid_to_sid( gid_t gid )
{
    SID_IDENTIFIER_AUTHORITY id;
    memset(&id, 0, sizeof(id));
    *(uint32_t *)(&id) = ((uint32_t)gid);
    id.Value[5] = 2;

    //BYTE subauth_count = 3;
    BYTE subauth_count = 5;
	/*DWORD subauthes[] = {
        SECURITY_LOGON_IDS_RID,
        0,
        1 /* FIXME: should be randomly generated when tokens are inherited by new processes 
    };*/
    DWORD subauthes[] = {
        SECURITY_NT_NON_UNIQUE,
		0,
		0,
		0,
		gid
	};
    return alloc_security_sid(&id, subauth_count, subauthes);
}
static int acl_is_valid( const ACL *acl, data_size_t size )
{
    ULONG i;
    const ACE_HEADER *ace;

    if (size < sizeof(ACL))
        return FALSE;

    size = min(size, MAX_ACL_LEN);

    size -= sizeof(ACL);

    ace = (const ACE_HEADER *)(acl + 1);
    for (i = 0; i < acl->AceCount; i++)
    {
        const SID *sid;
        data_size_t sid_size;

        if (size < sizeof(ACE_HEADER))
            return FALSE;
        if (size < ace->AceSize)
            return FALSE;
        size -= ace->AceSize;
        switch (ace->AceType)
        {
        case ACCESS_DENIED_ACE_TYPE:
            sid = (const SID *)&((const ACCESS_DENIED_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(ACCESS_DENIED_ACE, SidStart);
            break;
        case ACCESS_ALLOWED_ACE_TYPE:
            sid = (const SID *)&((const ACCESS_ALLOWED_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart);
            break;
        case SYSTEM_AUDIT_ACE_TYPE:
            sid = (const SID *)&((const SYSTEM_AUDIT_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(SYSTEM_AUDIT_ACE, SidStart);
            break;
        case SYSTEM_ALARM_ACE_TYPE:
            sid = (const SID *)&((const SYSTEM_ALARM_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(SYSTEM_ALARM_ACE, SidStart);
            break;
        case SYSTEM_MANDATORY_LABEL_ACE_TYPE:
            sid = (const SID *)&((const SYSTEM_MANDATORY_LABEL_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(SYSTEM_MANDATORY_LABEL_ACE, SidStart);
            break;
        default:
            return FALSE;
        }
        if (sid_size < FIELD_OFFSET(SID, SubAuthority[0]) || sid_size < security_sid_len( sid ))
            return FALSE;
        ace = ace_next( ace );
    }
    return TRUE;
}

/* checks whether all members of a security descriptor fit inside the size
 * of memory specified */
int sd_is_valid( const struct security_descriptor *sd, data_size_t size )
{
    size_t offset = sizeof(struct security_descriptor);
    const SID *group;
    const SID *owner;
    const ACL *sacl;
    const ACL *dacl;
    int dummy;

    if (size < offset)
        return FALSE;

    if ((sd->owner_len >= FIELD_OFFSET(SID, SubAuthority[255])) ||
        (offset + sd->owner_len > size))
        return FALSE;
    owner = sd_get_owner( sd );
    if (owner)
    {
        size_t needed_size = security_sid_len( owner );
        if ((sd->owner_len < sizeof(SID)) || (needed_size > sd->owner_len))
            return FALSE;
    }
    offset += sd->owner_len;

    if ((sd->group_len >= FIELD_OFFSET(SID, SubAuthority[255])) ||
        (offset + sd->group_len > size))
        return FALSE;
    group = sd_get_group( sd );
    if (group)
    {
        size_t needed_size = security_sid_len( group );
        if ((sd->group_len < sizeof(SID)) || (needed_size > sd->group_len))
            return FALSE;
    }
    offset += sd->group_len;

    if ((sd->sacl_len >= MAX_ACL_LEN) || (offset + sd->sacl_len > size))
        return FALSE;
    sacl = sd_get_sacl( sd, &dummy );
    if (sacl && !acl_is_valid( sacl, sd->sacl_len ))
        return FALSE;
    offset += sd->sacl_len;

    if ((sd->dacl_len >= MAX_ACL_LEN) || (offset + sd->dacl_len > size))
        return FALSE;
    dacl = sd_get_dacl( sd, &dummy );
    if (dacl && !acl_is_valid( dacl, sd->dacl_len ))
        return FALSE;
    offset += sd->dacl_len;

    return TRUE;
}

/* maps from generic rights to specific rights as given by a mapping */
static inline void map_generic_mask(unsigned int *mask, const GENERIC_MAPPING *mapping)
{
    if (*mask & GENERIC_READ) *mask |= mapping->GenericRead;
    if (*mask & GENERIC_WRITE) *mask |= mapping->GenericWrite;
    if (*mask & GENERIC_EXECUTE) *mask |= mapping->GenericExecute;
    if (*mask & GENERIC_ALL) *mask |= mapping->GenericAll;
    *mask &= 0x0FFFFFFF;
}

static inline int is_equal_luid( const LUID *luid1, const LUID *luid2 )
{
    return (luid1->LowPart == luid2->LowPart && luid1->HighPart == luid2->HighPart);
}

 inline void allocate_luid( luid_t *luid )
{
    prev_luid_value.low_part++;
    *luid = prev_luid_value;
}

DECL_HANDLER( allocate_locally_unique_id )
{
    allocate_luid( &reply->luid );
}

static inline void luid_and_attr_from_privilege( LUID_AND_ATTRIBUTES *out, const struct privilege *in)
{
    out->Luid = in->luid;
    out->Attributes =
        (in->enabled ? SE_PRIVILEGE_ENABLED : 0) |
        (in->def ? SE_PRIVILEGE_ENABLED_BY_DEFAULT : 0);
}

struct privilege *privilege_add( struct token *token, const LUID *luid, int enabled )
{
    struct privilege *privilege = mem_alloc( sizeof(*privilege) );
    if (privilege)
    {
        privilege->luid = *luid;
        privilege->def = privilege->enabled = (enabled != 0);
        list_add_tail( &token->privileges, &privilege->entry );
    }
    return privilege;
}

inline void privilege_remove( struct privilege *privilege )
{
    list_remove( &privilege->entry );
    free( privilege );
}

void token_destroy( struct object *obj )
{
    struct token* token;
    struct list *cursor, *cursor_next;

    assert( obj->ops == &token_ops );
    token = (struct token *)obj;

    free( token->user );

    LIST_FOR_EACH_SAFE( cursor, cursor_next, &token->privileges )
    {
        struct privilege *privilege = LIST_ENTRY( cursor, struct privilege, entry );
        privilege_remove( privilege );
    }

    LIST_FOR_EACH_SAFE( cursor, cursor_next, &token->groups )
    {
        struct group *group = LIST_ENTRY( cursor, struct group, entry );
        list_remove( &group->entry );
        free( group );
    }

    free( token->default_dacl );
}

/* creates a new token.
 *  groups may be NULL if group_count is 0.
 *  privs may be NULL if priv_count is 0.
 *  default_dacl may be NULL, indicating that all objects created by the user
 *   are unsecured.
 *  modified_id may be NULL, indicating that a new modified_id luid should be
 *   allocated.
 */
static struct token *create_token( unsigned primary, const SID *user,
                                   const SID_AND_ATTRIBUTES *groups, unsigned int group_count,
                                   const LUID_AND_ATTRIBUTES *privs, unsigned int priv_count,
                                   const ACL *default_dacl, TOKEN_SOURCE source,
                                   const luid_t *modified_id,
                                   int impersonation_level )
{
    struct token *token = alloc_object( &token_ops );
    if (token)
    {
        unsigned int i;

        allocate_luid( &token->token_id );
        if (modified_id)
            token->modified_id = *modified_id;
        else
            allocate_luid( &token->modified_id );
        list_init( &token->privileges );
        list_init( &token->groups );
        token->primary = primary;

        /* primary tokens don't have impersonation levels */
        if (primary)
            token->impersonation_level = -1;
        else
            token->impersonation_level = impersonation_level;
        
        token->default_dacl = NULL;
        token->primary_group = NULL;

        /* copy user */
       token->user = memdup( user, security_sid_len( user ));
        if (!token->user)
        {
            release_object( token );
            return NULL;
        }

        /* copy groups */
        for (i = 0; i < group_count; i++)
        {
            size_t size = FIELD_OFFSET( struct group, sid.SubAuthority[((const SID *)groups[i].Sid)->SubAuthorityCount] );
            struct group *group = mem_alloc( size );

            if (!group)
            {
                release_object( token );
                return NULL;
            }
            memcpy( &group->sid, groups[i].Sid, security_sid_len( groups[i].Sid ));
            group->enabled = TRUE;
            group->def = TRUE;
            group->logon = (groups[i].Attributes & SE_GROUP_LOGON_ID) != 0;
            group->mandatory = (groups[i].Attributes & SE_GROUP_MANDATORY) != 0;
            group->owner = (groups[i].Attributes & SE_GROUP_OWNER) != 0;
            group->resource = FALSE;
            group->deny_only = FALSE;
            list_add_tail( &token->groups, &group->entry );
            /* Use first owner capable group as an owner */
            if (!token->primary_group && group->owner)
                token->primary_group = &group->sid;
        }
        //assert( token->primary_group );

        /* copy privileges */
        for (i = 0; i < priv_count; i++)
        {
            /* note: we don't check uniqueness: the caller must make sure
             * privs doesn't contain any duplicate luids */
            if (!privilege_add( token, &privs[i].Luid,
                                privs[i].Attributes & SE_PRIVILEGE_ENABLED ))
            {
                release_object( token );
                return NULL;
            }
        }

        if (default_dacl)
        {
            token->default_dacl = memdup( default_dacl, default_dacl->AclSize );
            if (!token->default_dacl)
            {
                release_object( token );
                return NULL;
            }
        }

        token->source = source;
    }
    return token;
}

struct token *token_duplicate( struct token *src_token, unsigned primary,
                               int impersonation_level )
{
    const luid_t *modified_id =
        primary || (impersonation_level == src_token->impersonation_level) ?
            &src_token->modified_id : NULL;
    struct token *token = NULL;
    struct privilege *privilege;
    struct group *group;

    if (!primary &&
        (impersonation_level < SecurityAnonymous ||
         impersonation_level > SecurityDelegation ||
         (!src_token->primary && (impersonation_level > src_token->impersonation_level))))
    {
        set_error( STATUS_BAD_IMPERSONATION_LEVEL );
        return NULL;
    }

    token = create_token( primary, src_token->user, NULL, 0,
                          NULL, 0, src_token->default_dacl,
                          src_token->source, modified_id,
                          impersonation_level );
    if (!token) return token;

    /* copy groups */
    token->primary_group = NULL;
    LIST_FOR_EACH_ENTRY( group, &src_token->groups, struct group, entry )
    {
        size_t size = FIELD_OFFSET( struct group, sid.SubAuthority[group->sid.SubAuthorityCount] );
        struct group *newgroup = mem_alloc( size );
        if (!newgroup)
        {
            release_object( token );
            return NULL;
        }
        memcpy( newgroup, group, size );
        list_add_tail( &token->groups, &newgroup->entry );
        if (src_token->primary_group == &group->sid)
            token->primary_group = &newgroup->sid;
    }
    assert( token->primary_group );

    /* copy privileges */
    LIST_FOR_EACH_ENTRY( privilege, &src_token->privileges, struct privilege, entry )
        if (!privilege_add( token, &privilege->luid, privilege->enabled ))
        {
            release_object( token );
            return NULL;
        }

    return token;
}

//danqi
ACL *create_default_dacl( const SID *user )
{
    ACCESS_ALLOWED_ACE *aaa;
    ACL *default_dacl;
    SID *sid;
    size_t default_dacl_size = sizeof(ACL) +
                               2 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) +
                               sizeof(local_system_sid) +
                               security_sid_len( user );

    default_dacl = mem_alloc( default_dacl_size );
    if (!default_dacl) return NULL;

    default_dacl->AclRevision = ACL_REVISION;
    default_dacl->Sbz1 = 0;
    default_dacl->AclSize = default_dacl_size;
    default_dacl->AceCount = 2;
    default_dacl->Sbz2 = 0;

    /* GENERIC_ALL for Local System */
    aaa = (ACCESS_ALLOWED_ACE *)(default_dacl + 1);
    aaa->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    aaa->Header.AceFlags = 0;
    aaa->Header.AceSize = (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) +
                          sizeof(local_system_sid);
    aaa->Mask = GENERIC_ALL;
    sid = (SID *)&aaa->SidStart;
    memcpy( sid, &local_system_sid, sizeof(local_system_sid) );

    /* GENERIC_ALL for specified user */
    aaa = (ACCESS_ALLOWED_ACE *)((char *)aaa + aaa->Header.AceSize);
    aaa->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    aaa->Header.AceFlags = 0;
    aaa->Header.AceSize = (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) + security_sid_len( user );
    aaa->Mask = GENERIC_ALL;
    sid = (SID *)&aaa->SidStart;
    memcpy( sid, user, security_sid_len( user ));

    return default_dacl;
}

struct sid_data
{
    SID_IDENTIFIER_AUTHORITY idauth;
    int count;
    unsigned int subauth[MAX_SUBAUTH_COUNT];
};

// hyy
//zyq
struct token *first_token( uid_t unix_uid, gid_t unix_gid )
{
    SID* usid = security_unix_uid_to_sid(unix_uid);
    SID* gsid = security_unix_gid_to_sid(unix_gid);
    const SID_AND_ATTRIBUTES groups[] =
    {
        { security_world_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
        { security_local_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
        { security_interactive_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
        { security_authenticated_user_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
        { gsid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY|SE_GROUP_OWNER },
    };
    DWORD group_count = sizeof(groups) / sizeof(SID_AND_ATTRIBUTES);
	
    //traverse privilege array, find which privilege that gsid related to
	struct luidlistnode *syspriv_luids = new luidlist(); //store the privilege luid which find out
	for(struct privilege pr:sysprivs){
        sidlistnode *sids = pr->sid;
        while(sids){
            if(sids->val == gsid){
                luidlistaddtotail(syspriv_luids, pr->luid);
			}
			sids = sids->next;
		}
		
	}
	
	LUID_AND_ATTRIBUTES privs[] = {
		{ SeChangeNotifyPrivilege        , SE_PRIVILEGE_ENABLED },
        { SeSecurityPrivilege            , 0                    },
        { SeBackupPrivilege              , 0                    },
        { SeRestorePrivilege             , 0                    },
        { SeSystemtimePrivilege          , 0                    },
        { SeShutdownPrivilege            , 0                    },
        { SeRemoteShutdownPrivilege      , 0                    },
        { SeTakeOwnershipPrivilege       , 0                    },
        { SeDebugPrivilege               , 0                    },
        { SeSystemEnvironmentPrivilege   , 0                    },
        { SeSystemProfilePrivilege       , 0                    },
        { SeProfileSingleProcessPrivilege, 0                    },
        { SeIncreaseBasePriorityPrivilege, 0                    },
        { SeLoadDriverPrivilege          , SE_PRIVILEGE_ENABLED },
        { SeCreatePagefilePrivilege      , 0                    },
        { SeIncreaseQuotaPrivilege       , 0                    },
        { SeUndockPrivilege              , 0                    },
        { SeManageVolumePrivilege        , 0                    },
        { SeImpersonatePrivilege         , SE_PRIVILEGE_ENABLED },
        { SeCreateGlobalPrivilege        , SE_PRIVILEGE_ENABLED },
	};
	//traverse group privilege luid ==>syspriv_luids
	struct luidlistnode *luids = &syspriv_luids;
	for(LUID_AND_ATTRIBUTES priv: privs){
        while(luids){
            if(luids->val == priv->Luid)
				priv->Attributes = SE_PRIVILEGE_ENABLED;
			luids = luids->next;
		}
	}
	
    /*const LUID_AND_ATTRIBUTES admin_privs[] =
    {
        { SeChangeNotifyPrivilege        , SE_PRIVILEGE_ENABLED },
        { SeSecurityPrivilege            , 0                    },
        { SeBackupPrivilege              , SE_PRIVILEGE_ENABLED },
        { SeRestorePrivilege             , 0                    },
        { SeSystemtimePrivilege          , 0                    },
        { SeShutdownPrivilege            , 0                    },
        { SeRemoteShutdownPrivilege      , 0                    },
        { SeTakeOwnershipPrivilege       , 0                    },
        { SeDebugPrivilege               , 0                    },
        { SeSystemEnvironmentPrivilege   , 0                    },
        { SeSystemProfilePrivilege       , 0                    },
        { SeProfileSingleProcessPrivilege, 0                    },
        { SeIncreaseBasePriorityPrivilege, 0                    },
        { SeLoadDriverPrivilege          , SE_PRIVILEGE_ENABLED },
        { SeCreatePagefilePrivilege      , 0                    },
        { SeIncreaseQuotaPrivilege       , 0                    },
        { SeUndockPrivilege              , 0                    },
        { SeManageVolumePrivilege        , 0                    },
        { SeImpersonatePrivilege         , SE_PRIVILEGE_ENABLED },
        { SeCreateGlobalPrivilege        , SE_PRIVILEGE_ENABLED },
    };
    const LUID_AND_ATTRIBUTES common_privs[] =
    {
        { SeChangeNotifyPrivilege        , SE_PRIVILEGE_ENABLED },
        { SeSecurityPrivilege            , 0                    },
        { SeBackupPrivilege              , 0                    },
        { SeRestorePrivilege             , 0                    },
        { SeSystemtimePrivilege          , 0                    },
        { SeShutdownPrivilege            , 0                    },
        { SeRemoteShutdownPrivilege      , 0                    },
        { SeTakeOwnershipPrivilege       , 0                    },
        { SeDebugPrivilege               , 0                    },
        { SeSystemEnvironmentPrivilege   , 0                    },
        { SeSystemProfilePrivilege       , 0                    },
        { SeProfileSingleProcessPrivilege, 0                    },
        { SeIncreaseBasePriorityPrivilege, 0                    },
        { SeLoadDriverPrivilege          , SE_PRIVILEGE_ENABLED },
        { SeCreatePagefilePrivilege      , 0                    },
        { SeIncreaseQuotaPrivilege       , 0                    },
        { SeUndockPrivilege              , 0                    },
        { SeManageVolumePrivilege        , 0                    },
        { SeImpersonatePrivilege         , SE_PRIVILEGE_ENABLED },
        { SeCreateGlobalPrivilege        , SE_PRIVILEGE_ENABLED },
    };
    LUID_AND_ATTRIBUTES *privs = &admin_privs;
    if(unix_uid != getuid())
        privs = &common_privs; */

    DWORD priv_count = sizeof(privs) / sizeof(LUID_AND_ATTRIBUTES);
    const TOKEN_SOURCE source = {"SeMgr", {0U, 0}};
    ACL *default_dacl = create_default_dacl(usid);
    struct token *token = create_token(TRUE, usid, groups, group_count, privs, priv_count, default_dacl, source, NULL, 0);
    /*
    fprintf(stderr, "first_token uid=%u gid=%u usid=%#zx gsid=%#zx token->uid=%#zx token->gid=%#zx\n",
            unix_uid, unix_gid,
            (*(unsigned int *)(&(usid->IdentifierAuthority))),
            (*(unsigned int *)(&(gsid->IdentifierAuthority))),
            (*(unsigned int *)(&(token->user->IdentifierAuthority))),
            (*(unsigned int *)(&(token->primary_group->IdentifierAuthority)))
    );
    */
    return token;
}

static struct privilege *token_find_privilege( struct token *token, const LUID *luid, int enabled_only )
{
    struct privilege *privilege;
    LIST_FOR_EACH_ENTRY( privilege, &token->privileges, struct privilege, entry )
    {
        if (is_equal_luid( luid, &privilege->luid ))
        {
            if (enabled_only && !privilege->enabled)
                return NULL;
            return privilege;
        }
    }
    return NULL;
}

static unsigned int token_adjust_privileges( struct token *token, const LUID_AND_ATTRIBUTES *privs,
                                             unsigned int count, LUID_AND_ATTRIBUTES *mod_privs,
                                             unsigned int mod_privs_count )
{
    unsigned int i, modified_count = 0;

    /* mark as modified */
    allocate_luid( &token->modified_id );

    for (i = 0; i < count; i++)
    {
        struct privilege *privilege =
            token_find_privilege( token, &privs[i].Luid, FALSE );
        if (!privilege)
        {
            set_error( STATUS_NOT_ALL_ASSIGNED );
            continue;
        }

        if (privs[i].Attributes & SE_PRIVILEGE_REMOVED)
            privilege_remove( privilege );
        else
        {
            /* save previous state for caller */
            if (mod_privs_count)
            {
                luid_and_attr_from_privilege(mod_privs, privilege);
                mod_privs++;
                mod_privs_count--;
                modified_count++;
            }

            if (privs[i].Attributes & SE_PRIVILEGE_ENABLED)
                privilege->enabled = TRUE;
            else
                privilege->enabled = FALSE;
        }
    }
    return modified_count;
}

static void token_disable_privileges( struct token *token )
{
    struct privilege *privilege;

    /* mark as modified */
    allocate_luid( &token->modified_id );

    LIST_FOR_EACH_ENTRY( privilege, &token->privileges, struct privilege, entry )
        privilege->enabled = FALSE;
}

int token_check_privileges( struct token *token, int all_required,
                            const LUID_AND_ATTRIBUTES *reqprivs,
                            unsigned int count, LUID_AND_ATTRIBUTES *usedprivs)
{
    unsigned int i, enabled_count = 0;

    for (i = 0; i < count; i++)
    {
        struct privilege *privilege = 
            token_find_privilege( token, &reqprivs[i].Luid, TRUE );

        if (usedprivs)
            usedprivs[i] = reqprivs[i];

        if (privilege && privilege->enabled)
        {
            enabled_count++;
            if (usedprivs)
                usedprivs[i].Attributes |= SE_PRIVILEGE_USED_FOR_ACCESS;
        }
    }

    if (all_required)
        return (enabled_count == count);
    else
        return (enabled_count > 0);
}

int token_sid_present( struct token *token, const SID *sid, int deny )
{
    struct group *group;

    if (security_equal_sid( token->user, sid )) return TRUE;

    LIST_FOR_EACH_ENTRY( group, &token->groups, struct group, entry )
    {
        if (!group->enabled) continue;
        if (group->deny_only && !deny) continue;

        if (security_equal_sid( &group->sid, sid )) return TRUE;
    }

    return FALSE;
}

/* Checks access to a security descriptor. 'sd' must have been validated by
 * caller. It returns STATUS_SUCCESS if call succeeded or an error indicating
 * the reason. 'status' parameter will indicate if access is granted or denied.
 *
 * If both returned value and 'status' are STATUS_SUCCESS then access is granted.
 */
static unsigned int token_access_check( struct token *token,
                                 const struct security_descriptor *sd,
                                 unsigned int desired_access,
                                 LUID_AND_ATTRIBUTES *privs,
                                 unsigned int *priv_count,
                                 const GENERIC_MAPPING *mapping,
                                 unsigned int *granted_access,
                                 unsigned int *status )
{
    unsigned int current_access = 0;
    unsigned int denied_access = 0;
    ULONG i;
    const ACL *dacl;
    int dacl_present;
    const ACE_HEADER *ace;
    const SID *owner;

    /* assume no access rights */
    *granted_access = 0;

    /* fail if desired_access contains generic rights */
    if (desired_access & (GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE|GENERIC_ALL))
    {
        if (priv_count) *priv_count = 0;
        return STATUS_GENERIC_NOT_MAPPED;
    }

    dacl = sd_get_dacl( sd, &dacl_present );
    owner = sd_get_owner( sd );
    if (!owner || !sd_get_group( sd ))
    {
        if (priv_count) *priv_count = 0;
        return STATUS_INVALID_SECURITY_DESCR;
    }

    /* 1: Grant desired access if the object is unprotected */
    if (!dacl_present || !dacl)
    {
        if (priv_count) *priv_count = 0;
        *granted_access = desired_access;
        return *status = STATUS_SUCCESS;
    }

    /* 2: Check if caller wants access to system security part. Note: access
     * is only granted if specifically asked for */
    if (desired_access & ACCESS_SYSTEM_SECURITY)
    {
        const LUID_AND_ATTRIBUTES security_priv = { SeSecurityPrivilege, 0 };
        LUID_AND_ATTRIBUTES retpriv = security_priv;
        if (token_check_privileges( token, TRUE, &security_priv, 1, &retpriv ))
        {
            if (priv_count)
            {
                /* assumes that there will only be one privilege to return */
                if (*priv_count >= 1)
                {
                    *priv_count = 1;
                    *privs = retpriv;
                }
                else
                {
                    *priv_count = 1;
                    return STATUS_BUFFER_TOO_SMALL;
                }
            }
            current_access |= ACCESS_SYSTEM_SECURITY;
            if (desired_access == current_access)
            {
                *granted_access = current_access;
                return *status = STATUS_SUCCESS;
            }
        }
        else
        {
            if (priv_count) *priv_count = 0;
            *status = STATUS_PRIVILEGE_NOT_HELD;
            return STATUS_SUCCESS;
        }
    }
    else if (priv_count) *priv_count = 0;

    //zyq
	if(desired_access & GENERIC_ALL){
        const LUID_AND_ATTRIBUTES takeownership_priv = {SeTakeOwnershipPrivilege,0};
		LUID_AND_ATTRIBUTES retpriv = takeownership_priv;
	    if(token_check_privileges(token,TRUE,&takeownership_priv,1,&retpriv)){
            if(priv_count){
                if(*priv_count >= 1){
                    *priv_count = 1;
					*privs = retpriv;
				}
				else{
                    *priv_count = 1;
					return STATUS_BUFFER_TOO_SMALL;
				}
			}
			current_access |= GENERIC_ALL;
			if(desired_access == current_access){
                *granted_access = current_access;
				return *status = STATUS_SUCCESS;
			}
		}
		else{
            if(priv_count)
				*priv_count = 0;
			*status = STATUS_PRIVILEGE_NOT_HELD;
			return STATUS_SUCCESS;
		}
	}
	else if(priv_count)
		*priv_count = 0;

    /* 3: Check whether the token is the owner */
    /* NOTE: SeTakeOwnershipPrivilege is not checked for here - it is instead
     * checked when a "set owner" call is made, overriding the access rights
     * determined here. */
    if (token_sid_present( token, owner, FALSE ))
    {
        current_access |= (READ_CONTROL | WRITE_DAC);
        if (desired_access == current_access)
        {
            *granted_access = current_access;
            return *status = STATUS_SUCCESS;
        }
    }

    /* 4: Grant rights according to the DACL */
    ace = (const ACE_HEADER *)(dacl + 1);
    for (i = 0; i < dacl->AceCount; i++, ace = ace_next( ace ))
    {
        const ACCESS_ALLOWED_ACE *aa_ace;
        const ACCESS_DENIED_ACE *ad_ace;
        const SID *sid;

        if (ace->AceFlags & INHERIT_ONLY_ACE)
            continue;

        switch (ace->AceType)
        {
        case ACCESS_DENIED_ACE_TYPE:
            ad_ace = (const ACCESS_DENIED_ACE *)ace;
            sid = (const SID *)&ad_ace->SidStart;
            if (token_sid_present( token, sid, TRUE ))
            {
                unsigned int access = ad_ace->Mask;
                map_generic_mask(&access, mapping);
                if (desired_access & MAXIMUM_ALLOWED)
                    denied_access |= access;
                else
                {
                    denied_access |= (access & ~current_access);
                    if (desired_access & access) goto done;
                }
            }
            break;
        case ACCESS_ALLOWED_ACE_TYPE:
            aa_ace = (const ACCESS_ALLOWED_ACE *)ace;
            sid = (const SID *)&aa_ace->SidStart;
            if (token_sid_present( token, sid, FALSE ))
            {
                unsigned int access = aa_ace->Mask;
                map_generic_mask(&access, mapping);
                if (desired_access & MAXIMUM_ALLOWED)
                    current_access |= access;
                else
                    current_access |= (access & ~denied_access);
            }
            break;
        }

        /* don't bother carrying on checking if we've already got all of
            * rights we need */
        if (desired_access == *granted_access)
            break;
    }

done:
    if (desired_access & MAXIMUM_ALLOWED)
        *granted_access = current_access & ~denied_access;
    else
        if ((current_access & desired_access) == desired_access)
            *granted_access = current_access & desired_access;
        else
            *granted_access = 0;

    *status = *granted_access ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;

    return STATUS_SUCCESS;
}

static NTSTATUS token_object_access_check( struct token *token,
                                 const struct object *obj,
                                 unsigned int desired_access,
                                 const GENERIC_MAPPING *mapping,
                                 unsigned int *granted_access )
{
    unsigned int current_access = 0;
    unsigned int denied_access = 0;
    ULONG i;
    const struct security_descriptor *sd = obj->sd;
    const ACL *dacl;
    int dacl_present;
    const ACE_HEADER *ace;
    const SID *owner;

    /* assume no access rights */
    *granted_access = 0;

    /* fail if desired_access contains generic rights */
    if (desired_access & (GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE|GENERIC_ALL))
    {
        return STATUS_GENERIC_NOT_MAPPED;
    }

    dacl = sd_get_dacl( sd, &dacl_present );
    owner = sd_get_owner( sd );
    if (!owner || !sd_get_group( sd ))
    {
        return STATUS_INVALID_SECURITY_DESCR;
    }

    /* 1: Grant desired access if the object is unprotected */
    if (!dacl_present || !dacl)
    {
        *granted_access = desired_access;
        return STATUS_SUCCESS;
    }

    /* 2: Check if caller wants access to system security part. Note: access
     * is only granted if specifically asked for */
    if (desired_access & ACCESS_SYSTEM_SECURITY)
    {
        const LUID_AND_ATTRIBUTES security_priv = { SeSecurityPrivilege, 0 };
        LUID_AND_ATTRIBUTES retpriv = security_priv;
        if (token_check_privileges( token, TRUE, &security_priv, 1, &retpriv ))
        {
            current_access |= ACCESS_SYSTEM_SECURITY;
            if (desired_access == current_access)
            {
                *granted_access = current_access;
                return STATUS_SUCCESS;
            }
        }
        else
        {
            STATUS_PRIVILEGE_NOT_HELD;
            return STATUS_SUCCESS;
        }
    }

    /* 3: Check whether the token is the owner */
    /* NOTE: SeTakeOwnershipPrivilege is not checked for here - it is instead
     * checked when a "set owner" call is made, overriding the access rights
     * determined here. */
    if (token_sid_present( token, owner, FALSE ))
    {
        current_access |= (READ_CONTROL | WRITE_DAC);
        if (desired_access == current_access)
        {
            *granted_access = current_access;
            return STATUS_SUCCESS;
        }
    }

    /* 4: Grant rights according to the DACL */
    ace = (const ACE_HEADER *)(dacl + 1);
    for (i = 0; i < dacl->AceCount; i++, ace = ace_next( ace ))
    {
        const ACCESS_ALLOWED_ACE *aa_ace;
        const ACCESS_DENIED_ACE *ad_ace;
        const SID *sid;

        if (ace->AceFlags & INHERIT_ONLY_ACE)
            continue;

        switch (ace->AceType)
        {
        case ACCESS_DENIED_ACE_TYPE:
            ad_ace = (const ACCESS_DENIED_ACE *)ace;
            sid = (const SID *)&ad_ace->SidStart;
            //fprintf(stderr, "deny mask=%#x sid=%#zx\n", ad_ace->Mask, sid);
            if (token_sid_present( token, sid, TRUE ))
            {
                unsigned int access = ad_ace->Mask;
                map_generic_mask(&access, mapping);
                if (desired_access & MAXIMUM_ALLOWED)
                    denied_access |= access;
                else
                {
                    denied_access |= (access & ~current_access);
                    if (desired_access & access) goto done;
                }
            }
            break;
        case ACCESS_ALLOWED_ACE_TYPE:
            aa_ace = (const ACCESS_ALLOWED_ACE *)ace;
            sid = (const SID *)&aa_ace->SidStart;
            //fprintf(stderr, "allow mask=%#x sid=%#zx\n", aa_ace->Mask, sid);
            if (token_sid_present( token, sid, FALSE ))
            {
                unsigned int access = aa_ace->Mask;
                map_generic_mask(&access, mapping);
                if (desired_access & MAXIMUM_ALLOWED)
                    current_access |= access;
                else
                    current_access |= (access & ~denied_access);
            }
            break;
        }

        /* don't bother carrying on checking if we've already got all of
            * rights we need */
        if (desired_access == *granted_access)
            break;
    }

done:
    if (desired_access & MAXIMUM_ALLOWED)
        *granted_access = current_access & ~denied_access;
    else
        if ((current_access & desired_access) == desired_access)
            *granted_access = current_access & desired_access;
        else
            *granted_access = 0;

    if (!*granted_access) {
        fprintf(stderr, "sid=%#zx current=%x desired=%x denied=%x granted=%x\n",
            *(unsigned int *)&(token->user->IdentifierAuthority),
            current_access, desired_access, denied_access, *granted_access);
    }

    return *granted_access ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
}

const ACL *token_get_default_dacl( struct token *token )
{
    return token->default_dacl;
}

const SID *token_get_user( struct token *token )
{
    return token->user;
}

const SID *token_get_primary_group( struct token *token )
{
    return token->primary_group;
}

int check_object_access(struct object *obj, unsigned int *access)
{
    GENERIC_MAPPING mapping;
    struct token *token = current->token ? current->token : current->process->token;
    unsigned int status;
    int res;

    mapping.GenericAll = obj->ops->map_access( obj, GENERIC_ALL );
    if (!obj->sd)
    {
        if (*access & MAXIMUM_ALLOWED)
            *access = mapping.GenericAll;
        return TRUE;
    }

    mapping.GenericRead  = obj->ops->map_access( obj, GENERIC_READ );
    mapping.GenericWrite = obj->ops->map_access( obj, GENERIC_WRITE );
    mapping.GenericExecute = obj->ops->map_access( obj, GENERIC_EXECUTE );

    res = token_access_check( token, obj->sd, *access, NULL, NULL, 
                                &mapping, access, &status ) == STATUS_SUCCESS &&
                                status == STATUS_SUCCESS;

    if(!res)set_error(STATUS_ACCESS_DENIED);
    return TRUE;
}
int check_file_object_access(struct object *obj, unsigned int *access)
{
    GENERIC_MAPPING mapping;
    struct token *token = current->token ? current->token : current->process->token;

    mapping.GenericAll = obj->ops->map_access( obj, GENERIC_ALL );
    if (!obj->sd)
    {
        if (*access & MAXIMUM_ALLOWED)
            *access = mapping.GenericAll;
        return TRUE;
    }

    mapping.GenericRead  = obj->ops->map_access( obj, GENERIC_READ );
    mapping.GenericWrite = obj->ops->map_access( obj, GENERIC_WRITE );
    mapping.GenericExecute = obj->ops->map_access( obj, GENERIC_EXECUTE );

    const SID *usid = token->user;
    const SID *gsid = token->primary_group;
    if (usid) {
        //fprintf(stderr, "user usid=%#zx\n", (*(unsigned int *)(&(usid->IdentifierAuthority))));
    }
    if (gsid) {
        //fprintf(stderr, "user gsid=%#zx\n", (*(unsigned int *)(&(gsid->IdentifierAuthority))));
    }

    unsigned int status = STATUS_SUCCESS;
    unsigned int res;
    //hyy
    res = token_object_access_check( token, obj, *access, &mapping, access );
    //res = token_access_check( token, obj->sd, *access, NULL, NULL, &mapping, access, &status );
    if (!(res == STATUS_SUCCESS && status == STATUS_SUCCESS)) {
        //fprintf(stderr, "check res=%#x status=%#x\n", res, status);
        set_error( STATUS_ACCESS_DENIED );
        return FALSE;
    }
    return TRUE;
}


/* open a security token */
DECL_HANDLER(open_token)
{
    if (req->flags & OPEN_TOKEN_THREAD)
    {
        struct thread *thread = get_thread_from_handle( req->handle, 0 );
        if (thread)
        {
            if (thread->token)
            {
                if (!thread->token->primary && thread->token->impersonation_level <= SecurityAnonymous)
                    set_error( STATUS_CANT_OPEN_ANONYMOUS );
                else
                    reply->token = alloc_handle( current->process, thread->token,
                                                 req->access, req->attributes );
            }
            else
                set_error( STATUS_NO_TOKEN );
            release_object( thread );
        }
    }
    else
    {
        struct process *process = get_process_from_handle( req->handle, 0 );
        if (process)
        {
            if (process->token)
                reply->token = alloc_handle( current->process, process->token, req->access,
                                             req->attributes );
            else
                set_error( STATUS_NO_TOKEN );
            release_object( process );
        }
    }
}

/* adjust the privileges held by a token */
DECL_HANDLER(adjust_token_privileges)
{
    struct token *token;
    unsigned int access = TOKEN_ADJUST_PRIVILEGES;

    if (req->get_modified_state) access |= TOKEN_QUERY;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 access, &token_ops )))
    {
        const LUID_AND_ATTRIBUTES *privs = get_req_data();
        LUID_AND_ATTRIBUTES *modified_privs = NULL;
        unsigned int priv_count = get_req_data_size() / sizeof(LUID_AND_ATTRIBUTES);
        unsigned int modified_priv_count = 0;

        if (req->get_modified_state && !req->disable_all)
        {
            unsigned int i;
            /* count modified privs */
            for (i = 0; i < priv_count; i++)
            {
                struct privilege *privilege =
                    token_find_privilege( token, &privs[i].Luid, FALSE );
                if (privilege && req->get_modified_state)
                    modified_priv_count++;
            }
            reply->len = modified_priv_count;
            modified_priv_count = min( modified_priv_count, get_reply_max_size() / sizeof(*modified_privs) );
            if (modified_priv_count)
                modified_privs = set_reply_data_size( modified_priv_count * sizeof(*modified_privs) );
        }
        reply->len = modified_priv_count * sizeof(*modified_privs);

        if (req->disable_all)
            token_disable_privileges( token );
        else
            modified_priv_count = token_adjust_privileges( token, privs,
                priv_count, modified_privs, modified_priv_count );

        release_object( token );
    }
}

/* retrieves the list of privileges that may be held be the token */
DECL_HANDLER(get_token_privileges)
{
    struct token *token;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        int priv_count = 0;
        LUID_AND_ATTRIBUTES *privs;
        struct privilege *privilege;

        LIST_FOR_EACH_ENTRY( privilege, &token->privileges, struct privilege, entry )
            priv_count++;

        reply->len = priv_count * sizeof(*privs);
        if (reply->len <= get_reply_max_size())
        {
            privs = set_reply_data_size( priv_count * sizeof(*privs) );
            if (privs)
            {
                int i = 0;
                LIST_FOR_EACH_ENTRY( privilege, &token->privileges, struct privilege, entry )
                {
                    luid_and_attr_from_privilege( &privs[i], privilege );
                    i++;
                }
            }
        }
        else
            set_error(STATUS_BUFFER_TOO_SMALL);

        release_object( token );
    }
}

/* creates a duplicate of the token */
DECL_HANDLER(duplicate_token)
{
    struct token *src_token;

    if ((src_token = (struct token *)get_handle_obj( current->process, req->handle,
                                                     TOKEN_DUPLICATE,
                                                     &token_ops )))
    {
        struct token *token = token_duplicate( src_token, req->primary, req->impersonation_level );
        if (token)
        {
            reply->new_handle = alloc_handle( current->process, token, req->access, req->attributes);
            release_object( token );
        }
        release_object( src_token );
    }
}

/* checks the specified privileges are held by the token */
DECL_HANDLER(check_token_privileges)
{
    struct token *token;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        unsigned int count = get_req_data_size() / sizeof(LUID_AND_ATTRIBUTES);

        if (!token->primary && token->impersonation_level <= SecurityAnonymous)
            set_error( STATUS_BAD_IMPERSONATION_LEVEL );
        else if (get_reply_max_size() >= count * sizeof(LUID_AND_ATTRIBUTES))
        {
            LUID_AND_ATTRIBUTES *usedprivs = set_reply_data_size( count * sizeof(*usedprivs) );
            reply->has_privileges = token_check_privileges( token, req->all_required, get_req_data(), count, usedprivs );
        }
        else
            set_error( STATUS_BUFFER_OVERFLOW );
        release_object( token );
    }
}

/* checks that a user represented by a token is allowed to access an object
 * represented by a security descriptor */
DECL_HANDLER(access_check)
{
    data_size_t sd_size = get_req_data_size();
    const struct security_descriptor *sd = get_req_data();
    struct token *token;

    if (!sd_is_valid( sd, sd_size ))
    {
        set_error( STATUS_ACCESS_VIOLATION );
        return;
    }

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        GENERIC_MAPPING mapping;
        unsigned int status;
        LUID_AND_ATTRIBUTES priv;
        unsigned int priv_count = 1;

        memset(&priv, 0, sizeof(priv));

        /* only impersonation tokens may be used with this function */
        if (token->primary)
        {
            set_error( STATUS_NO_IMPERSONATION_TOKEN );
            release_object( token );
            return;
        }
        /* anonymous impersonation tokens can't be used */
        if (token->impersonation_level <= SecurityAnonymous)
        {
            set_error( STATUS_BAD_IMPERSONATION_LEVEL );
            release_object( token );
            return;
        }

        mapping.GenericRead = req->mapping_read;
        mapping.GenericWrite = req->mapping_write;
        mapping.GenericExecute = req->mapping_execute;
        mapping.GenericAll = req->mapping_all;

        status = token_access_check(
            token, sd, req->desired_access, &priv, &priv_count, &mapping,
            &reply->access_granted, &reply->access_status );

        reply->privileges_len = priv_count*sizeof(LUID_AND_ATTRIBUTES);

        if ((priv_count > 0) && (reply->privileges_len <= get_reply_max_size()))
        {
            LUID_AND_ATTRIBUTES *privs = set_reply_data_size( priv_count * sizeof(*privs) );
            memcpy( privs, &priv, sizeof(priv) );
        }

        set_error( status );
        release_object( token );
    }
}

/* retrieves the SID of the user that the token represents */
DECL_HANDLER(get_token_sid)
{
    struct token *token;

    reply->sid_len = 0;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        const SID *sid = NULL;

        switch (req->which_sid)
        {
        case TokenUser:
            assert(token->user);
            sid = token->user;
            break;
        case TokenPrimaryGroup:
            sid = token->primary_group;
            break;
        case TokenOwner:
        {
            struct group *group;
            LIST_FOR_EACH_ENTRY( group, &token->groups, struct group, entry )
            {
                if (group->owner)
                {
                    sid = &group->sid;
                    break;
                }
            }
            break;
        }
        default:
            set_error( STATUS_INVALID_PARAMETER );
            break;
        }

        if (sid)
        {
            reply->sid_len = security_sid_len( sid );
            if (reply->sid_len <= get_reply_max_size()) set_reply_data( sid, reply->sid_len );
            else set_error( STATUS_BUFFER_TOO_SMALL );
        }
        release_object( token );
    }
}

/* retrieves the groups that the user represented by the token belongs to */
DECL_HANDLER(get_token_groups)
{
    struct token *token;

    reply->user_len = 0;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        size_t size_needed = sizeof(struct token_groups);
        size_t sid_size = 0;
        unsigned int group_count = 0;
        const struct group *group;

        LIST_FOR_EACH_ENTRY( group, &token->groups, const struct group, entry )
        {
            group_count++;
            sid_size += security_sid_len( &group->sid );
        }
        size_needed += sid_size;
        /* attributes size */
        size_needed += sizeof(unsigned int) * group_count;

        /* reply buffer contains size_needed bytes formatted as:

           unsigned int count;
           unsigned int attrib[count];
           char sid_data[];

           user_len includes extra data needed for TOKEN_GROUPS representation,
           required caller buffer size calculated here to avoid extra server call */
        reply->user_len = FIELD_OFFSET( TOKEN_GROUPS, Groups[group_count] ) + sid_size;

        if (reply->user_len <= get_reply_max_size())
        {
            struct token_groups *tg = set_reply_data_size( size_needed );
            if (tg)
            {
                unsigned int *attr_ptr = (unsigned int *)(tg + 1);
                SID *sid_ptr = (SID *)(attr_ptr + group_count);

                tg->count = group_count;

                LIST_FOR_EACH_ENTRY( group, &token->groups, const struct group, entry )
                {

                    *attr_ptr = 0;
                    if (group->mandatory) *attr_ptr |= SE_GROUP_MANDATORY;
                    if (group->def) *attr_ptr |= SE_GROUP_ENABLED_BY_DEFAULT;
                    if (group->enabled) *attr_ptr |= SE_GROUP_ENABLED;
                    if (group->owner) *attr_ptr |= SE_GROUP_OWNER;
                    if (group->deny_only) *attr_ptr |= SE_GROUP_USE_FOR_DENY_ONLY;
                    if (group->resource) *attr_ptr |= SE_GROUP_RESOURCE;
                    if (group->logon) *attr_ptr |= SE_GROUP_LOGON_ID;

                    memcpy(sid_ptr, &group->sid, security_sid_len( &group->sid ));

                    sid_ptr = (SID *)((char *)sid_ptr + security_sid_len( &group->sid ));
                    attr_ptr++;
                }
            }
        }
        else set_error( STATUS_BUFFER_TOO_SMALL );

        release_object( token );
    }
}

DECL_HANDLER(get_token_impersonation_level)
{
    struct token *token;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        if (token->primary)
            set_error( STATUS_INVALID_PARAMETER );
        else
            reply->impersonation_level = token->impersonation_level;

        release_object( token );
    }
}

DECL_HANDLER(get_token_statistics)
{
    struct token *token;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        reply->token_id = token->token_id;
        reply->modified_id = token->modified_id;
        reply->primary = token->primary;
        reply->impersonation_level = token->impersonation_level;
        reply->group_count = list_count( &token->groups );
        reply->privilege_count = list_count( &token->privileges );

        release_object( token );
    }
}

DECL_HANDLER(get_token_default_dacl)
{
    struct token *token;

    reply->acl_len = 0;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        if (token->default_dacl)
            reply->acl_len = token->default_dacl->AclSize;

        if (reply->acl_len <= get_reply_max_size())
        {
            ACL *acl_reply = set_reply_data_size( reply->acl_len );
            if (acl_reply)
                memcpy( acl_reply, token->default_dacl, reply->acl_len );
        }
        else set_error( STATUS_BUFFER_TOO_SMALL );

        release_object( token );
    }
}

DECL_HANDLER(set_token_default_dacl)
{
    struct token *token;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_ADJUST_DEFAULT,
                                                 &token_ops )))
    {
        const ACL *acl = get_req_data();
        unsigned int acl_size = get_req_data_size();

        free( token->default_dacl );
        token->default_dacl = NULL;

        if (acl_size)
            token->default_dacl = memdup( acl, acl_size );

        release_object( token );
    }
}

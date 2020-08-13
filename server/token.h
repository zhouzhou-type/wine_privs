/*
 * Wine server threads
 *
 * Copyright (C) 1998 Alexandre Julliard
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

#ifndef __WINE_SERVER_TOKEN_H
#define __WINE_SERVER_TOKEN_H

#include "object.h"

extern const SID world_sid;
extern const SID local_sid;
extern const SID interactive_sid;
extern const SID anonymous_logon_sid;
extern const SID authenticated_user_sid;
extern const SID local_system_sid;

extern const PSID security_local_sid;
extern const PSID security_interactive_sid;
extern const PSID security_authenticated_user_sid;

struct local_user_sid_def/* same fields as struct SID */
{
    BYTE Revision;
    BYTE SubAuthorityCount;
    SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
    DWORD SubAuthority[5];
} ;

extern struct local_user_sid_def local_user_sid;

struct builtin_admins_sid_def/* same fields as struct SID */
{
    BYTE Revision;
    BYTE SubAuthorityCount;
    SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
    DWORD SubAuthority[2];
} ;

extern struct builtin_admins_sid_def builtin_admins_sid;

struct builtin_users_sid_def/* same fields as struct SID */
{
    BYTE Revision;
    BYTE SubAuthorityCount;
    SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
    DWORD SubAuthority[2];
};


extern struct builtin_users_sid_def builtin_users_sid;


extern luid_t prev_luid_value;

struct token
{
    struct object  obj;             /* object header */
    luid_t         token_id;        /* system-unique id of token */
    luid_t         modified_id;     /* new id allocated every time token is modified */
    struct list    privileges;      /* privileges available to the token */
    struct list    groups;          /* groups that the user of this token belongs to (sid_and_attributes) */
    SID           *user;            /* SID of user this token represents */
    SID           *primary_group;   /* SID of user's primary group */
    unsigned       primary;         /* is this a primary or impersonation token? */
    ACL           *default_dacl;    /* the default DACL to assign to objects created by this user */
    TOKEN_SOURCE   source;          /* source of the token */
    int            impersonation_level; /* impersonation level this token is capable of if non-primary token */
    int            session_id;
};

//zyq
struct privilege
{
    struct list entry;
    LUID        luid;
    unsigned    enabled  : 1; /* is the privilege currently enabled? */
    unsigned    def      : 1; /* is the privilege enabled by default? */
	//struct list sid;
    struct sidlistnode* sid;
};

//zyq
struct group
{
    struct list entry;
    unsigned    enabled  : 1; /* is the sid currently enabled? */
    unsigned    def      : 1; /* is the sid enabled by default? */
    unsigned    logon    : 1; /* is this a logon sid? */
    unsigned    mandatory: 1; /* is this sid always enabled? */
    unsigned    owner    : 1; /* can this sid be an owner of an object? */
    unsigned    resource : 1; /* is this a domain-local group? */
    unsigned    deny_only: 1; /* is this a sid that should be use for denying only? */
    SID         sid;
	LPWSTR      lgrpi1_name;
	LPWSTR      lgrpi1_comment;
};

//zyq
struct user{
    struct list entry;
	LPWSTR      usri1_name;
	LPWSTR      usri1_password;
    DWORD       usri1_password_age;
    DWORD       usri1_priv;
	SID         sid;
};

//zyq
struct user_group_relations{
    struct list entry;
	SID         u_sid;
	SID         g_sid;
};


extern void token_dump( struct object *obj, int verbose );
extern void token_destroy( struct object *obj );
extern inline void privilege_remove( struct privilege *privilege );
extern unsigned int token_map_access( struct object *obj, unsigned int access );
extern inline void allocate_luid( luid_t *luid );
extern struct privilege *privilege_add( struct token *token, const LUID *luid, int enabled );
extern SID *security_sid_alloc( const SID_IDENTIFIER_AUTHORITY *idauthority, int subauthcount, const unsigned int subauth[] );
extern void token_destroy( struct object *obj );
extern ACL *create_default_dacl( const SID *user );


extern const struct object_ops token_ops;




#endif  /* __WINE_SERVER_TOKEN_H */

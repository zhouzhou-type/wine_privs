/*
 * Declarations for operators
 *
 * 
 */

#include <wintrust.h>



extern NTSTATUS rebase_perform_relocations( void *module, SIZE_T len );
extern NTSTATUS operators_rebase( void *module, SIZE_T len,const IMAGE_DATA_DIRECTORY *relocs );



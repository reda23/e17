/** @file etk_argument.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Evas.h>
#include "etk_utils.h"
#include "etk_argument.h"

#define ETK_ARGUMENT_FLAG_PRIV_SET (1 << 4)

static Evas_Hash *_etk_argument_extra = NULL;
static int _etk_argument_status = 0;

/**
 * @brief Parses the arguments as described by the user
 * @param args the arguments you are interested in
 * @param argc the number of arguments given to your program
 * @param argv the values of the arguments given to your program
 * @return Returns an int which tells the caller about the status
 *
 * Example:
 * @code
 * static void info_func(Etk_Argument *args, int index)
 * {
 *   printf("info func!\n");
 * }
 *
 * static void text_func(Etk_Argument *args, int index)
 * {
 *    printf("text func! %s\n", args[index].data);
 * }
 *
 * static void hide_func(Etk_Argument *args, int index)
 * {
 *   printf("hide func!\n");
 * }
 *
 * Etk_Argument args[] = {
 *      { "info", 'i', NULL, info_func, NULL, ETK_ARGUMENT_FLAG_OPTIONAL, " " },
 *      { "text", 't', NULL, text_func, NULL, ETK_ARGUMENT_FLAG_OPTIONAL|ETK_ARGUMENT_FLAG_VALUE_REQUIRED, " " },
 *      { "hide", 'h', NULL, hide_func, NULL, ETK_ARGUMENT_FLAG_OPTIONAL, " " },
 *      { NULL,   -1,  NULL, NULL,      NULL, ETK_ARGUMENT_FLAG_NONE,     " " }
 * };
 *
 * int main(int argc, char **argv)
 * {
 *    etk_arguments_parse(args, argc, argv);
 *
 *    return 0;
 * }
 * @endcode
 */
int etk_arguments_parse(Etk_Argument *args, int argc, char **argv)
{
   int i;
   Etk_Argument *arg;
      
   /* no arguments */
   if(argc < 2)
   {
      /* check for required arguments */
      i = 0;
      arg = args;	     
      while(arg->short_name != -1)
      {
	 if(arg->flags & ETK_ARGUMENT_FLAG_REQUIRED)
	 {
	    printf(_("Argument %d '-%c | --%s' is required\n"), i, arg->short_name, arg->long_name);
	    return ETK_ARGUMENT_RETURN_REQUIRED_NOT_FOUND;
	 }
	 ++i; ++arg;
      }
      return ETK_ARGUMENT_RETURN_OK_NONE_PARSED;
   }
   
   for(i = 1; i < argc; i++)
   {
      char *cur;
      
      cur = argv[i];
      if(!cur) continue;
      
      /* min length is 2, anything less is invalid */
      if(strlen(cur) < 2 && cur[0] == '-')
      {
	 printf(_("Argument %d '%s' is too short\n"), i, argv[i]);
	 return ETK_ARGUMENT_RETURN_MALFORMED;
      }

      /* short (single char) argument of the form -d val or -dval */
      if(cur[0] == '-' && cur[1] != '-')
      {
	 arg = args;
	 
	 while(arg->short_name != -1)
	 {
	    /* match found for short arg */
	    if(arg->short_name == cur[1])
	    {
	       /* check to see if arg needs value */
	       if((arg->flags & ETK_ARGUMENT_FLAG_VALUE_REQUIRED) &&
		  i + 1 < argc)
	       {
		  char *val = argv[i + 1];
		  
		  /* if no value is present, report error */
		  if(val[0] == '-')
		  {
		     printf(_("Argument %d '%s' requires a value\n"), i, cur);
		     return ETK_ARGUMENT_RETURN_REQUIRED_VALUE_NOT_FOUND;
		  }
		  
		  arg->data = evas_list_append(arg->data, val);
		  arg->flags |= ETK_ARGUMENT_FLAG_PRIV_SET;
		  _etk_argument_status = 1;
		  ++i;
	       }
	       else if (arg->flags & ETK_ARGUMENT_FLAG_VALUE_REQUIRED
			&& i + 1 >= argc)
	       {
		  /* if no value is present, report error */
		  printf(_("Argument %d '%s' requires a value\n"), i, cur);
		  return ETK_ARGUMENT_RETURN_REQUIRED_VALUE_NOT_FOUND;
	       }
	       else if(!(arg->flags & ETK_ARGUMENT_FLAG_VALUE_REQUIRED))
	       {
		  arg->flags |= ETK_ARGUMENT_FLAG_PRIV_SET;
		  _etk_argument_status = 1;
	       }
		  
	    }
	    ++arg;
	 }
      }
      /* long argument of the form --debug or --debug=something */
      else if(cur[0] == '-' && cur[1] == '-' && strlen(cur) > 2)
      {
	 arg = args;
	 
	 while(arg->short_name != -1)
	 {
	    char *tmp = NULL;
	    char *tmp2;
	    
	    if(!arg->long_name)
	      continue;
	    
	    /* check if arg if of the form --foo=bar */
	    tmp = strchr(cur, '=');
	    if(tmp)
	    {
	       tmp2 = cur;
	       cur = calloc(tmp - tmp2 + 1, sizeof(char));
	       snprintf(cur, (tmp - tmp2 + 1) * sizeof(char), "%s", tmp2);		     		       
	    }
	    else		    
	      tmp = NULL;
	    
	    /* match found for long arg */
	    if(!strcmp(arg->long_name, cur + 2))
	    {		       
	       /* check to see if arg needs value */
	       if((arg->flags & ETK_ARGUMENT_FLAG_VALUE_REQUIRED) &&
		  ((i + 1 < argc) || (tmp != NULL)))
	       {
		  char *val;
		  
		  if(!tmp)
		    val = argv[i + 1];
		  else
		    val = tmp + 1;			    
		  
		  /* if no value is present, report error */
		  if(val[0] == '-')
		  {
		     printf(_("Argument %d '%s' requires a value\n"), i, cur);
		     return ETK_ARGUMENT_RETURN_REQUIRED_VALUE_NOT_FOUND;
		  }
		  
		  arg->data = evas_list_append(arg->data, val);
		  arg->flags |= ETK_ARGUMENT_FLAG_PRIV_SET;
		  _etk_argument_status = 1;		  
		  
		  if(!tmp)
		    ++i;
	       }
	       else if (arg->flags & ETK_ARGUMENT_FLAG_VALUE_REQUIRED
			&& i + 1 >= argc)
	       {
		  /* if no value is present, report error */
		  printf(_("Argument %d '%s' requires a value\n"), i, cur);
		  return ETK_ARGUMENT_RETURN_REQUIRED_VALUE_NOT_FOUND;
	       }
	       else if(!(arg->flags & ETK_ARGUMENT_FLAG_VALUE_REQUIRED))
	       {
		  arg->flags |= ETK_ARGUMENT_FLAG_PRIV_SET;
		  _etk_argument_status = 1;
	       }		  
	    }
	    
	    if(tmp)
	    {
	       free(cur);
	       cur = argv[i];
	    }
	    	    
	    if(arg->flags & ETK_ARGUMENT_FLAG_MULTIVALUE && i + 1 < argc &&
	       arg->short_name != -1 && arg->flags & ETK_ARGUMENT_FLAG_PRIV_SET)
	    {
	       /* if we want multi-argument arguments like:
		* foo --bar "one" "two" "three"
		* then this is where we get them.
		*/
	       char *extra;
	       Evas_List *value = NULL;
	       int j = 1;
	       
	       extra = argv[i + j];
	       while(i + j < argc)
	       {
		  if(extra[0] == '-')
		  {
		     j = argc;
		     break;
		  }
		  
		  if(arg->long_name != NULL)
		    value = evas_hash_find(_etk_argument_extra, arg->long_name);
		  else if(arg->short_name != ' ' && arg->short_name != -1)
		    value = evas_hash_find(_etk_argument_extra, &arg->short_name);
		  else
		    break;
		  
		  if(!value)
		  {
		     value = evas_list_append(value, extra);
		     _etk_argument_extra = evas_hash_add(_etk_argument_extra, arg->long_name ? arg->long_name : &arg->short_name, value);
		  }
		  else
		  {
		     _etk_argument_extra = evas_hash_del(_etk_argument_extra, arg->long_name ? arg->long_name : &arg->short_name, value);
		     value = evas_list_append(value, extra);
		     _etk_argument_extra = evas_hash_add(_etk_argument_extra, arg->long_name ? arg->long_name : &arg->short_name, value);
		  }
		  
		  ++j;
		  extra = argv[i + j];
	       }
	    }
	    
	    ++arg;	    
	 }
      }      
   }
   
   /* check for required arguments */
   i = 0;
   arg = args;	     
   while(arg->short_name != -1)
   {
      if(!(arg->flags & ETK_ARGUMENT_FLAG_PRIV_SET) &&
	 arg->flags & ETK_ARGUMENT_FLAG_REQUIRED)
      {
	 printf(_("Argument %d '-%c | --%s' is required\n"), i, arg->short_name, arg->long_name);
	 return ETK_ARGUMENT_RETURN_REQUIRED_NOT_FOUND;
      }
      ++i; ++arg;
   }
   
   /* call all the callbacks */
   i = 0;
   arg = args;	     
   while(arg->short_name != -1)
   {
      if(arg->func && arg->flags & ETK_ARGUMENT_FLAG_PRIV_SET)
	arg->func(args, i);
      ++i; ++arg;
   }

   if(_etk_argument_status == 0)     
     return ETK_ARGUMENT_RETURN_OK_NONE_PARSED;     
   else
     return ETK_ARGUMENT_RETURN_OK;
}

void etk_argument_help_show(Etk_Argument *args)
{
   Etk_Argument *arg;
   
   arg = args;
   while(arg->short_name != -1)
     {
	if(arg->long_name)
	  printf("--%s ", arg->long_name);
	if(arg->short_name != -1 && arg->short_name != ' ')
	  printf("-%c", arg->short_name);
	printf("\t");
	if(arg->description)
	  printf("%s", arg->description);
	printf("\n");
	++arg;
     }
}

Evas_List *etk_argument_extra_find(const char *key)
{
   if(!_etk_argument_extra)
     return NULL;
   
   return evas_hash_find(_etk_argument_extra, "column");
}

/** @} */

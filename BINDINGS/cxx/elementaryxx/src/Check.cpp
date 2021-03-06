#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include "../include/elementaryxx/Check.h"

using namespace std;

namespace Elmxx {

Check::Check (Evasxx::Object &parent)
{
  o = elm_check_add (parent.obj ());
  
  elmInit ();
}

Check::~Check () {}

Check *Check::factory (Evasxx::Object &parent)
{
  return new Check (parent);
}

void Check::setState (bool state)
{
  elm_check_state_set (o, state);
}

bool Check::getState () const
{
  return elm_check_state_get (o);
}

} // end namespace Elmxx

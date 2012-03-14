#include <stdlib.h>

/*
 * This splits a string into X parts given the split character.
 */
int splitString (char *string, char *items[], char splitChar)
{
   /* Declare local variables.  */
   char temp[128];
   int stringLength = 0;
   int chunks = 0;
   int pos = 0;
   int x;

   /* Check if the given string is NULL.  */
   if (string == (char *)NULL)
   {
      return (0);
   }

   /* Get the length of the string.  */
   stringLength = (int)strlen (string);
   if (stringLength == 0)
   {
      return (0);
   }

   /* Zero out the temp string. */
   //memset (temp, 0, sizeof(temp));

   /* Start looking for the string. */
   for (x=0; x < stringLength; x++)
   {
      if (string[x] == splitChar)
      {
         /* Copy the string into the array. */
         temp[pos++] = 0;
         items[chunks++] = strdup (temp);

         /* Zero out the temp string.  */
         memset (temp, sizeof(temp), '\0');

         /* Reset the position counter.  */
         pos = 0;
      }
      else
      {
         temp[pos++] = string[x];
      }
   }

   /* Don't forget the last one.  */
   temp[pos++] = 0;
   items[chunks++] = strdup (temp);
   return chunks;
}

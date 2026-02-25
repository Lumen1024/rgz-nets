#include <ui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vars.h>

int getusername(char *username)
{
  if (!fgets(username, NAME_LEN, stdin))
    return 1;
  username[strcspn(username, "\r\n")] = '\0';
  if (!*username)
  {
    printf("Имя не может быть пустым.\n");
    return 1;
  }
  return 0;
}

int getchoice(int *choice, int count)
{
  char buf[8];
  if (!fgets(buf, sizeof(buf), stdin))
    return 1;

  *choice = atoi(buf);
  if (*choice < 0 || *choice >= count)
    return 1;
  return 0;
}

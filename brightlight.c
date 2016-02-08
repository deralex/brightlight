/* brightlight v1 - change the screen backlight brightness on Linux systems
** Copyright (C) 2016 David Miller <multiplexd@gmx.com>
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
** This program requires libbsd <libbsd.freedesktop.org> or a BSD-compatible 
** implementation of strlcpy() and strlcat().
**
** To compile:
**
**   $ gcc -o brightlight brightlight.c -lbsd
**
** (assuming you use gcc of course; clang works as well). This program can 
** also be statically linked by adding the -static flag.
*/


#include <bsd/string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLIGHT_PATH "/sys/class/backlight/intel_backlight"
#define MAX_PATH_LEN 200
#define EXTRA_PATH_LEN 20
#define PROGRAM_NAME "brightlight"
#define PROGRAM_VERSION 1

unsigned int get_backlight;
unsigned int set_backlight;
unsigned int max_brightness;
int brightness;               /* Signed because of file I/O testing done in read_maximum_brightness() which requires signed ints */
unsigned int maximum;
unsigned int values_as_percentages;
char *argv0;
char backlight_path[MAX_PATH_LEN];

unsigned int from_percentage(unsigned int val_to_convert);
unsigned int get_current_brightness();
unsigned int get_max_brightness();
void parse_args(int argc, char* argv[]);
unsigned int parse_cmdline_int(char* arg_to_parse);
void read_backlight_brightness();
void read_maximum_brightness();
void set_current_brightness(unsigned int bright);
unsigned int to_percentage(unsigned int val_to_convert);
void usage();
void validate_args();
void validate_control_directory();
void version();
void write_backlight_brightness();

int main(int argc, char* argv[]) {

   argv0 = argv[0];

   parse_args(argc, argv);

   validate_control_directory();

   maximum = get_max_brightness();

   if(set_backlight) {
      validate_args();
   }

   if(get_backlight) {
      read_backlight_brightness();
   } else if(set_backlight) {
      write_backlight_brightness();
   } else if(max_brightness) {
      read_maximum_brightness();
   } else {
      fputs("Error parsing options. Pass the -h flag for help.\n", stderr);
   }

   exit(0);
}

unsigned int from_percentage(unsigned int val_to_convert) {

   return (val_to_convert * maximum) / 100;
}

unsigned int get_current_brightness() {
   int bright = -1;
   FILE* brightness_file;
   char path[MAX_PATH_LEN + EXTRA_PATH_LEN];

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, "/brightness", MAX_PATH_LEN + EXTRA_PATH_LEN);

   brightness_file = fopen(path, "r");
   if(brightness_file == NULL) {
      fputs("Error occured while trying to open brightness file.\n", stderr);
      exit(1);
   }

   fscanf(brightness_file, "%d", &bright);
   if(bright < 0) {
      fputs("Could not read brightness from brightness file.\n", stderr);
      exit(1);
   }

   fclose(brightness_file);

   return (unsigned int) bright;
}

unsigned int get_max_brightness() {
   int max = -1;
   FILE* max_file;
   char path[MAX_PATH_LEN + EXTRA_PATH_LEN];

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, "/max_brightness", MAX_PATH_LEN + EXTRA_PATH_LEN);

   max_file = fopen(path, "r");
   if(max_file == NULL) {
      fputs("Error occured while trying to open max_brightness file.\n", stderr);
      exit(1);
   }

   fscanf(max_file, "%d", &max);
   if(max < 0) {
      fputs("Could not read maximum brightness from max_brightness file.\n", stderr);
      exit(1);
   }

   fclose(max_file);

   return (unsigned int) max;
}

void parse_args(int argc, char* argv[]) {
   /*
   ** This function, specifically the conditional loop and immediately following 
   ** if statement, is based on the function parse_args() in thttpd.c of thttpd-2.27 
   ** by Jef Poskanzer (http://www.acme.com/software/thttpd).
   */
   int argn, conflicting_args;
   char* cmdline_brightness;

   set_backlight = 0;
   get_backlight = 0;
   max_brightness = 0;
   values_as_percentages = 0;
   brightness = 0;
   strlcpy(backlight_path, BACKLIGHT_PATH, MAX_PATH_LEN);     /* Use the compiled-in default path unless told otherwise */
   conflicting_args = 0;

   if(argc == 1) {
      fputs("No options specified. Pass the -h flag for help.\n", stderr);
      exit(1);
   }

   argn = 1;
   while(argn < argc && argv[argn][0] == '-') {
      if(strcmp(argv[argn], "-v") == 0) {
         version();
         exit(0);
      } else if(strcmp(argv[argn], "-h") == 0) {
         usage();
         exit(0);
      } else if(strcmp(argv[argn], "-r") == 0) {
         if(conflicting_args) {
            fputs("Conflicting options given! Pass the -h flag for help.\n", stderr);
            exit(1);
         }
         get_backlight = 1;
         conflicting_args = 1;
      } else if(strcmp(argv[argn], "-w") == 0 && argn + 1 < argc) {
         if(conflicting_args) {
            fputs("Conflicting options given! Pass the -h flag for help.\n", stderr);
            exit(1);
         }
         argn++;
         set_backlight = 1;
         cmdline_brightness = argv[argn];
         conflicting_args = 1;
      } else if(strcmp(argv[argn], "-f") == 0 && argn + 1 < argc) {
         argn++;
         strlcpy(backlight_path, argv[argn], MAX_PATH_LEN);
      } else if(strcmp(argv[argn], "-p") == 0) {
         values_as_percentages = 1;
      } else if(strcmp(argv[argn], "-m") == 0) {
         if(conflicting_args) {
            fputs("Conflicting options given! Pass the -h flag for help.\n", stderr);
            exit(1);
         }
         max_brightness = 1;
         conflicting_args = 1;
      } else {
         fputs("Error parsing options. Pass the -h flag for help.\n", stderr);
         exit(1);
      }
      argn++;
   }
   if(argn != argc) {
      fputs("Error parsing options. Pass the -h flag for help.\n", stderr);
      exit(1);
   }

   if(set_backlight)
      brightness = parse_cmdline_int(cmdline_brightness);

   return;
}

unsigned int parse_cmdline_int(char* arg_to_parse) {
   int character = 0;

   while(arg_to_parse[character] != '\0') {
      if(character >= 5 || isdigit(arg_to_parse[character]) == 0) {
         fputs("Invalid argument. Pass the -h flag for help.\n", stderr);
         exit(1);
      }
      character++;
   }

   return (unsigned int) atoi(arg_to_parse);
}

void read_backlight_brightness() {
   unsigned int outval;
   char* out_string_end;

   brightness = get_current_brightness();

   if(values_as_percentages) {
      outval = to_percentage(brightness);
      out_string_end = "%.";
   } else {
      outval = brightness;
      out_string_end = ".";
   }

   printf("Current backlight brightness is: %d%s\n", outval, out_string_end);

   return;
}

void read_maximum_brightness() {
   unsigned int outval;
   char* out_string_end;

   if(values_as_percentages) {
      outval = to_percentage(maximum);
      out_string_end = "%.";
   } else {
      outval = maximum;
      out_string_end = ".";
   }

   printf("Maximum backlight brightness is: %d%s\n", outval, out_string_end);

   return;
}

void set_current_brightness(unsigned int bright) {
   FILE* brightness_file;
   char path[MAX_PATH_LEN + EXTRA_PATH_LEN];

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, "/brightness", MAX_PATH_LEN + EXTRA_PATH_LEN);

   brightness_file = fopen(path, "w");
   if(brightness_file == NULL) {
      fputs("Error occured while trying to open brightness file.\n", stderr);
      exit(1);
   }

   if(fprintf(brightness_file, "%d", bright) < 0) {
      fputs("Could not write brightness to brightness file.\n", stderr);
      exit(1);
   }

   fclose(brightness_file);

   return;
}

unsigned int to_percentage(unsigned int val_to_convert) {

   return (val_to_convert * 100) / maximum;
}

void usage() {
   printf("Usage: %s [OPTIONS]\n", argv0);
   printf("\
Options:\n\
\n\
      -v         Print program version and exit.\n\
      -h         Show this help message.\n\
      -p         Read or write the brightness level as a percentage (0 to 100)\n\
                 instead of the internal scale the kernel uses (such as e.g. 0\n\
                 to 7812).\n\
      -r         Read the backlight brightness level.\n\
      -w <val>   Set the backlight brightness level to <val>, where <val> is a\n\
                 a positive integer.\n\
      -f <path>  Specify alternative path to backlight control directory, such\n\
                 as \"/sys/class/backlight/intel_backlight/\"\n\
      -m         Show maximum brightness level of the screen backlight on the \n\
                 kernel's scale. The compile-time default control directory is\n\
                 used if -f is not specified. The -p flag is ignored when this\n\
                 option is specified.\n\n");

   printf("The flags -r, -w and -m are mutually exclusive, however one of the three is \nrequired.\n");

   return;
}

void validate_args() {
/* Don't check if brightness is an int, already done by parse_cmdline_int() */

   if(values_as_percentages) {
      if(brightness < 0 || brightness > 100) {
         fputs("Invalid argument. Pass the -h flag for help.\n", stderr);
         exit(1);
      }
   } else {
      if(brightness < 0 || brightness > maximum) {
         fputs("Invalid argument. Pass the -h flag for help.\n", stderr);
         exit(1);
      }
   }


   return;
}

void validate_control_directory() {
   DIR* control_dir;
   char path[MAX_PATH_LEN + EXTRA_PATH_LEN];

   control_dir = opendir(backlight_path);

   if(control_dir == NULL) {
      if(errno == ENOTDIR) {
         fprintf(stderr, "%s is not a directory.\n", backlight_path);
      } else if(errno == EACCES) {
         fprintf(stderr, "Could not access %s: Permission denied.\n", backlight_path);
      } else {
         fputs("Could not access control directory.\n", stderr);
         exit(1);
      }
   }

   closedir(control_dir);

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, "/brightness", EXTRA_PATH_LEN);

   if(access(path, F_OK) != 0) {
      fputs("Control directory exists but could not find brightness control file.\n", stderr);
      exit(1);
   }

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, "/max_brightness", EXTRA_PATH_LEN);

   if(access(path, F_OK) != 0) {
      fputs("Control directory exists but could not find max_brightness file.\n", stderr);
      exit(1);
   }
   return;
}

void version() {
   printf("%s v%d\n", PROGRAM_NAME, PROGRAM_VERSION);
   puts("Copyright (C) 2016 David Miller <multiplexd@gmx.com");
   printf("\
This is free software under the terms of the GNU General Public License, \n\
version 2 or later. You are free to use, modify and redistribute it, however \n\
there is NO WARRANTY; please see <https://gnu.org/licenses/gpl.html> for \n\
further information.\n");
}

void write_backlight_brightness() {
   unsigned int oldval;
   unsigned int val_to_write;
   unsigned int current;
   char* out_string_end;
   char* out_string_filler;

   current = get_current_brightness();

   if(values_as_percentages) {
      val_to_write = from_percentage(brightness);
      oldval = to_percentage(current);
      out_string_end = "%.";
      out_string_filler = "% ";
   } else {
      val_to_write = brightness;
      oldval = current;
      out_string_end = ".";
      out_string_filler = " ";
   }

   set_current_brightness(val_to_write);

   printf("Changed backlight brightness: %d%s=> %d%s\n", oldval, out_string_filler, brightness, out_string_end);      

   return;
}
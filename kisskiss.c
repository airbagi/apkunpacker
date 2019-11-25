/*
 * Unpacker for various Android packers/protectors
 *
 * Tim 'diff' Strazzere <strazz@gmail.com>
 * modified by Zend (2019)
 *"
 * For education use only - find them malwares
 *
 *
 * Tested against malware packed with the following;
 *     - Bangcle (All versions)
 *     - APKProtect (native versions w/ crypto)
 *     - LIAPP (prereleased demo)
 *     - Qihoo Packer
 *     - Jiagu
 *
 *
 * This will dump the dex or optimized dex (odex) files from
 * the system, meaning you will have to pull the
 * framework files to be able to deodex (in needed)
 * for the original dex file.
 *
 * For APKProtect, it should be combined with the
 * LD_PRELOAD hook'er to avoid the QEMU detection
 * if running on an emulator
 */

#include "kisskiss.h"

char *package_name = NULL;

int main(int argc, char *argv[])
  {

  printf("[*] Android Dalvik Unpacker modified by Zend (c) 2019\n");

  if(argc <= 1)
    {
    printf(" [!] Nothing to unpack, quitting\n");
    return 0;
    }

  if(getuid() != 0)
    {
    printf(" [!] Not root, quitting\n");
    return -1;
    }

  package_name = argv[1];
  printf(" [+] Hunting for %s\n", package_name);

  uint32_t pid = get_process_pid(package_name);
  if(pid == 0)
    {
    printf(" [!] Process could not be found!\n");
    return -1;
    }
  printf(" [+] %d is service pid\n", pid);

  uint32_t tracer = checkTracer(pid);

  uint32_t clone_pid = get_clone_pid(pid);
  if(clone_pid == 0)
    {
    printf(" [!] A suitable clone process could not be found!");
    return -1;
    }
  printf(" [+] %d is clone pid\n", clone_pid);

  uint32_t clone_tracer = checkTracer(clone_pid);

  int mem_file = attach_get_memory(clone_pid);
  if(mem_file == -1)
    {
    perror(" [!] An error occurred attaching and finding the memory ");
    return -1;
    }

  // Determine if we are dealing with APKProtect or Bangcle
  packer *found_packer = determine_packer(clone_pid, mem_file);
  if(found_packer == NULL)
    {
    printf("  [*] No packer found on clone_pid %d, falling back to service_pid %d\n", clone_pid, pid);
    mem_file = attach_get_memory(pid);
    if(mem_file == -1)
      {
      perror(" [!] An error occurred attaching and finding the memory ");
      return -1;
      }

    found_packer = determine_packer(pid, mem_file);
    }
  //
  if(found_packer != NULL && strcmp(found_packer->name, "Bangle Test") == 0)
    {
    printf("  [+] Since filter is Bangle Test, switching to look at the pid attached to service_pid, %d\n", tracer);
    clone_pid = tracer;
    mem_file = attach_get_memory(clone_pid);
    if(mem_file == -1)
      {
      perror(" [!] An error occurred attaching and finding the memory ");
      return -1;
      }
    }
  //
  char *filter = NULL;
  if(found_packer != NULL)
    {
    filter = found_packer->filter;
    }

  memory_region *memory[128] = { true, 0, 0 };
  int found = find_magic_memory(clone_pid, mem_file, memory, filter);
  if(found <= 0)
    {
    printf(" [!] Something unexpected happened, new version of packer/protectors? Or it wasn't packed/protected!\n");
    return -1;
    }
  //
  char class_path[strlen(package_name)];
  strncpy(class_path, package_name, strlen(package_name) + 1);
  replaceAll(class_path, (char) '.', (char) '/');
  //
  printf(" [+] Found %d potentially interesting memory locations...\n", found);
  int output = 0;
  for(int i = 0; i < found; i++)
    {
    // Build a safe file to dump to and call the memory dumping function
    char *dumped_file_name = malloc(strlen(static_safe_location) + strlen(package_name) + strlen(suffix) + 1 /* _ */ + (found == 0 ? 1 : (int) (log10(found) + 1) + 1));
    if (memory[i]->bNewFile) output++;
    sprintf(dumped_file_name, "%s%s_%d%s", static_safe_location, package_name, output, suffix );
    int result = dump_memory(class_path, mem_file, memory[i], dumped_file_name, (found_packer != NULL));
    if(result < 0)
      {
      perror(" [!] An issue occurred trying to dump the memory to a file ");
      return -1;
      }
    else if(result == 0)
      {
      // Potential system dex file
      }
    else
      {
      printf(" [+] Unpacked/protected file dumped to : %s\n", dumped_file_name);
      //output++;
      }
    }
  close(mem_file);
  ptrace(PTRACE_DETACH, clone_pid, NULL, 0);
  return 1;
  }

/* binary search in memory */
int memsearch(const char *hay, int haysize, const char *needle, int needlesize)
  {
  int haypos, needlepos;
  haysize -= needlesize;
  for (haypos = 0; haypos <= haysize; haypos++)
    {
    for (needlepos = 0; needlepos < needlesize; needlepos++)
      {
      if (hay[haypos + needlepos] != needle[needlepos])
        {
        // Next character in haystack.
        break;
        }
      }
    if (needlepos == needlesize)
      {
      return haypos;
      }
    }
  return -1;
  }

uint32_t checkTracer(uint32_t pid)
  {
  char status[1024];
  snprintf(status, sizeof(status), "/proc/%u/status", pid);
  FILE *status_file = NULL;

  if((status_file = fopen(status, "r")) == NULL)
    {
    perror("  [!] Unable to check status of pid ");
    return -1;
    }

  char key[1024];
  uint32_t value;
  uint32_t tracerpid = 0;
  while(fscanf(status_file, "%s\t%u\n", key, &value) >= 0)
    {
    if(strcmp(key, "TracerPid:") == 0 && value != 0)
      {
      printf("  [*] Warning, process %u seems to be traced by %d, likely an anti-debug trick!\n", pid, value);
      tracerpid = value;
      }
    }

  fclose(status_file);
  return tracerpid;
  }

void replaceAll(char* str, char oldChar, char newChar)
  {
  int i = 0;
  while(str[i] != '\0')
    {
    if(str[i] == oldChar)
      str[i] = newChar;
    i++;
    }
  }

int checkFd(int fd)
  {
  return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
  }

/*
 * Since most of these tools provide "anti-debugging" features using ptrace,
 * we are going to take advantage of the Android app lifecycle and just steal
 * the memory form a cloned process which is never ptraced.
 *
 * This function will simply recurse through the given pids /proc/pid/task/
 * directory and collect the last one, which has always worked in tests done.
 */
uint32_t get_clone_pid(uint32_t service_pid)
  {
  DIR *service_pid_dir;
  char service_pid_directory[1024];
  sprintf(service_pid_directory, "/proc/%d/task/", service_pid);

  if((service_pid_dir = opendir(service_pid_directory)) == NULL)
    return 0;

  struct dirent* directory_entry = NULL;
  struct dirent* last_entry = NULL;
  while((directory_entry = readdir(service_pid_dir)) != NULL)
    {
    last_entry = directory_entry;
    }

  if(last_entry == NULL)
    return 0;

  closedir(service_pid_dir);
  return atoi(last_entry->d_name);
  }

/*
 * Using a known package name, recurse through the /proc/pid
 * directory and look at the cmdline for the package name, this
 * should give us the "parent" pid for any package we are looking
 * for, which is then referenced as "service_id"
 */
uint32_t get_process_pid(const char *target_package_name)
  {
  char self_pid[10];
  sprintf(self_pid, "%u", getpid());

  DIR *proc = NULL;

  if((proc = opendir("/proc")) == NULL)
    return 0;

  struct dirent *directory_entry = NULL;
  while((directory_entry = readdir(proc)) != NULL)
    {

    if (directory_entry == NULL)
      return 0;

    // We don't care if it's self or our own pid
    if (strcmp(directory_entry->d_name, "self") == 0 ||
        strcmp(directory_entry->d_name, self_pid) == 0)
      continue;

    char cmdline[1024];
    snprintf(cmdline, sizeof(cmdline), "/proc/%s/cmdline", directory_entry->d_name);
    FILE *cmdline_file = NULL;
    // Attempt to iterate to next one if failed...
    if((cmdline_file = fopen(cmdline, "r")) == NULL)
      continue;

    char process_name[1024];
    fscanf(cmdline_file, "%s", process_name);

    fclose(cmdline_file);

    if(strcmp(process_name, target_package_name) == 0)
      {
      closedir(proc);
      return atoi(directory_entry->d_name);
      }
    }

  closedir(proc);
  return 0;
  }

/*
 * Extremely weak filtering process, looks for known shared libs
 * that are mapped to memory.
 */
packer *determine_packer(uint32_t clone_pid, int memory_fd)
  {
  char maps[1024];
  snprintf(maps, sizeof(maps), "/proc/%d/maps", clone_pid);

  printf(" [+] Attempting to detect packer/protector...\n");

  FILE *maps_file = NULL;
  if((maps_file = fopen(maps, "r")) == NULL)
    return NULL;

  // Scan the /proc/pid/maps file and currently hardcoded shared lib names
  // scan for : /data/dalvik-cache/x86/data@app@com.illuminate.texaspoker-1@
  char mem_line[2048];
  while(fscanf(maps_file, "%[^\n]\n", mem_line) >= 0)
    {
    // Iterate through all markers to find proper filter
    int i;
    char strfmtMarker[1024];

    for(i = 0; i < sizeof(packers) / sizeof(packers[0]); i++)
      {
      sprintf(strfmtMarker, packers[i].marker, package_name);
      if(strstr(mem_line, strfmtMarker) != NULL)
        {
        printf("  [*] Found %s\n", packers[i].name);
        return &packers[i];
        }
      }
    }
  printf("  [*] Nothing special found, hunting for all dex and odex magic bytes...\n");

  return NULL;
  }

/*
 * Find the "magic" memory location we want, usually an odex so we are currently
 * recursing through the /proc/pid/maps and peeopling at memory locations using
 * the peek_memory function.
 *
 * returns number of interesting memory locations found
 */
int find_magic_memory(uint32_t clone_pid, int memory_fd, memory_region *memory[], char *extra_filter)
  {
  char maps[1024];
  snprintf(maps, sizeof(maps), "/proc/%d/maps", clone_pid);

  FILE *maps_file = NULL;
  if((maps_file = fopen(maps, "r")) == NULL)
    return -1;

  // Scan the /proc/pid/maps file and find possible memory of interest
  // Currently this loops until we find the /last/ odex file which is usually correct
  char mem_line[1024];
  int found = 0;

  while(fscanf(maps_file, "%[^\n]\n", mem_line) >= 0)
    {
    // For APKProtect we want the odex file that is mapped to memory
    // so we are looking for an extra_filter match on the odex
    if((extra_filter != NULL) && (strstr(mem_line, extra_filter) == NULL))
      {
      continue;
      }
    printf("%s", mem_line);

    // Otherwise we are looking for the location that bangcle allocates the odex to,
    // this is a very ugly way to just try and get the directly mapped meory
    /*
        if(extra_filter == NULL && (strstr(mem_line, "/") != NULL || strstr(mem_line, "[") != NULL))
          continue;
      */
    char mem_address_start[10];
    char mem_address_end[10];
    sscanf(mem_line, "%8[^-]-%8[^ ]", mem_address_start, mem_address_end);

    uint64_t mem_start = strtoul(mem_address_start, NULL, 16);

    // Peek and see if the memory is what we wanted
    off64_t offset = peek_memory(memory_fd, mem_start);
    // If we had a filter and didn't find a dex or odex, just return it anyway
    // This allows us to return non-dex file things (like an apk, jar or zip)
    if(extra_filter != NULL && offset < 0)
      offset = 0;

    if(offset >= 0)
      {
      printf("found dex at 0x%llx\n", offset);
      //
      if ((offset>0) && (found>0)) // first one skipped
        {
        memory[found] = malloc(sizeof(memory_region));
        memory[found]->start = mem_start;
        memory[found]->end = mem_start+offset;
        memory[found]->bNewFile = false;
        found++;
        }
      // Found a magic
      memory[found] = malloc(sizeof(memory_region));
      memory[found]->start = mem_start + offset;
      memory[found]->bNewFile = (offset > 0);
      memory[found]->end = strtoull(mem_address_end, NULL, 16);
      found++;
      }
    else if(offset == -99)
      {
      // No offset found
      }
    else
      {
      perror(" [!] Error peeking at memory ");
      }
    }

  fclose(maps_file);
  return found;
  }

// Just peek at the memory to see if it contains an odex we want
// return value is the offset into memory of magic being found
off64_t peek_memory(int memory_file, uint64_t address)
  {
#define BUFFER_SIZE 0x5000
  char buffer[BUFFER_SIZE];
  printf("peekmem %llx\n", address);

  int read = pread64(memory_file, buffer, BUFFER_SIZE, address);
  if (read < 0)
    {
    fprintf(stderr, "  [!] pread seems to have failed at 0x%llx\n", address);
    return -1;
    }

  // We are currently just dumping odex or jar files, letting the packers/protectors do all
  // the heavy lifting for us


  off64_t result = memsearch(buffer, read, dex_magic, strlen(dex_magic));
  if(result != -1)
    return result;

  result = memsearch(buffer, read, odex_magic, strlen(odex_magic));
  if(result != -1)
    return result;

  return -99;
  }

/*
 * Dump a given memory location via a file descriptor, "memory_region"
 * and a given file_name for output.
 */
int dump_memory(char* class_path, int memory_fd, memory_region *memory, const char *file_name, int ignore_class_path)
  {
  int ret = 0;
  char *buffer = malloc(memory->end - memory->start);

  printf(" [+] Attempting to search inside memory region 0x%llx to 0x%llx\n", memory->start, memory->end);

  if(checkFd < 0)
    {
    perror("  [!] Appears to be an issue with memory fd ");
    return -1;
    }

  ssize_t read = pread64(memory_fd, buffer, (size_t)(memory->end - memory->start), (off64_t)(memory->start));
  if(read < 0)
    {
    fprintf(stderr, "  [!] pread seems to have failed for 0x%llx to 0x%llx\n", memory->start, memory->end);
    return -1;
    }

  if((memory->end - memory->start) != read)
    {
    printf("  [!] pread did not read the expected amount of memory!\n");
    return -1;
    }
  /*
  off64_t *contained_offset = memmem(buffer, (size_t)(memory->end - memory->start), class_path, strlen(class_path));

  FILE *dump = NULL;

  if(contained_offset != NULL || ignore_class_path == 1) {
    if(contained_offset != NULL)
      printf("  [+] Memory region 0x%llx to 0x%llx contained anticipated class path %s\n", memory->start, memory->end, class_path);
  */
  FILE *dump;
  if (memory->bNewFile)
    dump = fopen(file_name, "wb");
  else
    dump = fopen(file_name, "ab");

  ret = -1;
  if(dump != NULL)
    {
    if(fwrite(buffer, memory->end - memory->start, 1, dump) > 0)
      {
      ret = 1;
      }

    fclose(dump);
    }
  else
    {
    perror("  [!] Error opening a file to write ");
    }
  /*
    } else {
      printf("  [-] Likely a system file found, ignoring...\n");
      ret = 0;
    }
    */
  free(buffer);
  return ret;
  }

// Perform all that ptrace magic
int attach_get_memory(uint32_t pid)
  {
  char mem[8000];
  snprintf(mem, sizeof(mem), "/proc/%d/mem", pid);

  // Attach to process so we can peek/dump
  if (0 != ptrace(PTRACE_ATTACH, pid, NULL, NULL))
    return -1;

  // Get the mem file so we can read when we want too
  int mem_file;
  if(!(mem_file = open(mem, O_RDONLY)))
    return -1;

  return mem_file;
  }

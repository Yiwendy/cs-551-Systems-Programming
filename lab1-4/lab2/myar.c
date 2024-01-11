#define _POSIX_SOURCE
#include <ar.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
struct meta {
  char name[16]; // room for null
  int mode;
  int size;
  time_t mtime; // a time_t is a long
};
int fill_ar_hdr(char *filename, struct ar_hdr *hdr) {
  struct stat status;
  if (stat(filename, &status) == -1) {
    return 1;
  }
  if (!S_ISREG(status.st_mode)) {
    return 1;
  }
  strncpy(hdr->ar_name, filename, 16);
  sprintf(hdr->ar_date, "%ld", status.st_mtime);
  sprintf(hdr->ar_uid, "%d", status.st_uid);
  sprintf(hdr->ar_gid, "%d", status.st_gid);
  sprintf(hdr->ar_mode, "%o", status.st_mode);
  sprintf(hdr->ar_size, "%lu", status.st_size);
  memcpy(hdr->ar_fmag, ARFMAG, sizeof(hdr->ar_fmag));
  return 0;
}
int fill_meta(struct ar_hdr hdr, struct meta *meta) {
  strncpy(meta->name, hdr.ar_name, 16);
  sscanf(hdr.ar_date, "%ld", &meta->mtime);
  sscanf(hdr.ar_mode, "%o", &meta->mode);
  sscanf(hdr.ar_size, "%d", &meta->size);
  return 0;
}
int check_header(int archive) {
  lseek(archive, 0, SEEK_SET);
  void *magic_buffer[SARMAG];
  read(archive, magic_buffer, SARMAG);
  if (memcmp(ARMAG, magic_buffer, SARMAG) != 0) {
    return 1;
  }
  return 0;
}
int prepare_archive(char *filename) {
  int archive = open(filename, O_RDWR | O_CREAT, 0666);
  if (archive == -1) {
    return -1;
  }
  struct stat status;
  fstat(archive, &status);
  if (status.st_size == 0) {
    write(archive, ARMAG, SARMAG);
  } else {
    if (check_header(archive) != 0) {
      close(archive);
      return -1;
    }
  }
  return archive;
}
int append_file(int archive, char *filename) {
  struct ar_hdr header;
  if (fill_ar_hdr(filename, &header) == 1) {
    return 1;
  }
  struct meta meta;
  fill_meta(header, &meta);
  int file = open(filename, O_RDONLY);
  void *file_data = mmap(NULL, meta.size, PROT_READ, MAP_PRIVATE, file, 0);
  close(file);
  write(archive, &header, sizeof(header));
  write(archive, file_data, meta.size);
  if (meta.size & 1) {
    write(archive, &meta, 1);
  }
  munmap(file_data, meta.size);
  return 0;
}
int Q(char *argv[]) {
  int archive = prepare_archive(argv[0]);
  if (archive == -1) {
    return 1;
  }
  lseek(archive, 0, SEEK_END);
  while (*(++argv) != NULL) {
    if (append_file(archive, argv[0]) != 0) {
      return 1;
    }
  }
  close(archive);
  return 0;
}
int X(int o, char *argv[]) {
  int archive = open(argv[0], O_RDONLY);
  if (archive == -1) {
    return 1;
  }
  ++argv;
  if (check_header(archive) != 0) {
    close(archive);
    return 1;
  }
  struct ar_hdr header;
  while (read(archive, &header, sizeof(header)) == sizeof(header)) {
    struct meta meta;
    fill_meta(header, &meta);
    for (char **target = argv; *target != NULL; ++target) {
      if (strcmp(target[0], header.ar_name) != 0) {
        continue;
      }
      int target = open(header.ar_name, O_WRONLY | O_TRUNC | O_CREAT, 0666);
      if (target == -1) {
        return 1;
      }
      char *buffer = malloc(meta.size);
      read(archive, buffer, meta.size);
      lseek(archive, -meta.size, SEEK_CUR);
      write(target, buffer, meta.size);
      close(target);
      free(buffer);
      if (o) {
        chmod(header.ar_name, meta.mode);
        struct utimbuf buf = {.actime = meta.mtime, .modtime = meta.mtime};
        utime(header.ar_name, &buf);
      }
      break;
    }
    off_t real_size = meta.size + (meta.size & 1);
    lseek(archive, real_size, SEEK_CUR);
  }
  close(archive);
  return 0;
}
int T(int v, char *argv[]) {
  int archive = open(argv[0], O_RDONLY);
  if (archive == -1) {
    return 1;
  }
  if (check_header(archive) != 0) {
    close(archive);
    return 1;
  }
  struct ar_hdr header;
  while (read(archive, &header, sizeof(header)) == sizeof(header)) {
    struct meta meta;
    fill_meta(header, &meta);
    if (v) {
      printf(
          "%s\t%s/%s\t%s\t%s\t%s\n", header.ar_mode, header.ar_uid, header.ar_gid, header.ar_size,
          header.ar_date, header.ar_name
      );
    } else {
      printf("%s\n", header.ar_name);
    }
    off_t real_size = meta.size + (meta.size & 1);
    lseek(archive, real_size, SEEK_CUR);
  }
  close(archive);
  return 0;
}
int D(char *argv[]) {
  int archive = open(argv[0], O_RDONLY);
  if (archive == -1) {
    return 1;
  }
  if (check_header(archive) != 0) {
    close(archive);
    return 1;
  }
  unlink(argv[0]);
  int new_archive = prepare_archive(argv[0]);
  if (new_archive == -1) {
    ////
    close(archive);
    return 1;
  }
  struct ar_hdr header;
  ++argv;
  while (read(archive, &header, sizeof(header)) == sizeof(header)) {
    int append = 1;
    struct meta meta;
    fill_meta(header, &meta);
    off_t real_size = meta.size + (meta.size & 1);
    for (char **target = argv; *target != NULL; ++target) {
      if (strcmp(target[0], header.ar_name) == 0) {
        append = 0;
        break;
      }
    }
    if (append) {
      char *buffer = malloc(real_size);
      read(archive, buffer, real_size);
      write(new_archive, &header, sizeof(header));
      write(new_archive, buffer, real_size);
      lseek(archive, -real_size, SEEK_CUR);
      free(buffer);
    }
    lseek(archive, real_size, SEEK_CUR);
  }
  close(new_archive);
  close(archive);
  return 0;
}
int a(int A, char *argv[]) {
  int archive = prepare_archive(argv[0]);
  if (archive == -1) {
    return 1;
  }
  lseek(archive, 0, SEEK_END);
  DIR *dir = opendir(".");
  while (1) {
    struct dirent *entry = readdir(dir);
    if (entry == NULL) {
      break;
    }
    struct stat status;
    stat(entry->d_name, &status);
    if (!S_ISREG(status.st_mode)) {
      continue;
    }
    time_t current_time = time(NULL);
    if (current_time - status.st_mtime <= (time_t)60 * 60 * 24 * A) {
      continue;
    }
    append_file(archive, entry->d_name);
  }
  closedir(dir);
  close(archive);
  return 0;
}
int main(int argc, char *argv[]) {
  int q = 0;
  int x = 0;
  int o = 0;
  int t = 0;
  int v = 0;
  int d = 0;
  int A = -1;
  char *helper;
  if (argc == 1) {
    return 1;
  }
  ++argv;
  while (argv[0] != NULL) {
    if (argv[0][0] != '-') {
      break;
    }
    int i = 1;
    while (argv[0][i] != '\0') {
      switch (argv[0][i]) {
      case 'q':
        q = 1;
        ++i;
        break;
      case 'x':
        x = 1;
        ++i;
        break;
      case 'o':
        o = 1;
        ++i;
        break;
      case 't':
        t = 1;
        ++i;
        break;
      case 'v':
        v = 1;
        ++i;
        break;
      case 'd':
        d = 1;
        ++i;
        break;
      case 'A':
        if (argv[0][i + 1] != '\0') {
          A = strtol(argv[0] + i + 1, &helper, 10);
          i = helper - argv[0];
        } else {
          if (argv[1] == NULL) {
            return 1;
          }
          A = strtol(argv[1], NULL, 10);
          ++argv;
          i = 0;
          argv[0][i] = '\0';
        }
        break;
      default:
        return 1;
      }
    }
    ++argv;
  }
  if (argv[0] == NULL) {
    return 1;
  }
  int ops = q + x + t + d;
  if (ops > 1 || (ops == 1 && A != -1)) {
    return 1;
  }
  if ((o && !x) || (v && !t)) {
    return 1;
  }
  if (q) {
    return Q(argv);
  } else if (x) {
    return X(o, argv);
  } else if (t) {
    return T(v, argv);
  } else if (d) {
    return D(argv);
  } else if (A) {
    return a(A, argv);
  }
  return 1;
}
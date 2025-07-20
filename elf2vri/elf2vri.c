/*

  ELF2VRI converter 2025-06-18 VLSI Solution.

  Requires readelf to be installed if you need to convert elf files.
  Not required for binary files.

  This source code may be freely used and modified as long as it has
  something to do with VLSI Solution's RISC-V chips.

  Compile like this:
  gcc -g -Wall -Wformat-overflow=0 -funsigned-char -O -o elf2vri elf2vri.c -lm

 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>


#if 1
#define USE_BSS_COMPRESSION
#endif

#define MAX_SECTIONS 1024

#define STR_SIZE 1024

#define MAX_RAW_FILES 256

/*
   Test data 2024-03-07, running at 12.288 MHz for
   /work/hv32/riscv-linux-boot/riscv-linux-boot.elf
   Original ELF: 38524592 bytes.
   Original .vri without optimization: 34306440 bytes.

   RLE Limit   File size/B  Load time/s
           8      17978312        9.295
           a      17977520        9.212
          10      17986464        9.073
          20      18023936        8.953
          40      18047208        8.916
          80      18063768        8.924
         100      18075176        8.913
         200      18085512        8.914
         400      18099168        8.930
        1000      18142200        8.928
*/
#define RLE_RUN_LIMIT_32BIT_WORDS 0x80

#define FL_READ    1
#define FL_WRITE   2
#define FL_EXECUTE 4
#define FL_BSS     8

struct Section {
  unsigned int addr;
  unsigned int fileSize;
  unsigned int memSize;
  unsigned int flags;
  char *name;
};

enum Phase {
  phNone,
  phProgramHeaders,
  phSegmentSections,
};

int inSize = 0, outSize = 0;
int rawInSize = 0, rawOutSize = 0;
int currentAddress = 0;
int autoAddress = 1;

void Put32HeadbangEndian(FILE *fp, unsigned int i) {
  fprintf(fp, "%c%c%c%c",
	  (i>>16)&0xFF, (i>>24)&0xFF, (i>>0)&0xFF, (i>>8)&0xFF);
  outSize += 4;
}

void Put32Swapped(FILE *fp, unsigned int i) {
  fprintf(fp, "%c%c%c%c",
	  (i>>0)&0xFF, (i>>8)&0xFF, (i>>16)&0xFF, (i>>24)&0xFF);
  outSize += 4;
}

void Put32MixedEndian(FILE *fp, unsigned int i) {
  fprintf(fp, "%c%c%c%c",
	  (i>>8)&0xFF, (i>>0)&0xFF, (i>>24)&0xFF, (i>>16)&0xFF);
  outSize += 4;
}


unsigned int *mySectData = NULL;
int sectWords = 0;

void PrepareSection(unsigned int fileSize) {
  mySectData = calloc(fileSize, sizeof(mySectData[0]));
  sectWords = 0;
}

void AddSection32(unsigned int d) {
  mySectData[sectWords++] = d;
}

unsigned int PurgeSections(struct Section *s, FILE *fp, unsigned int *d, unsigned int addr, int literalRun, int rleRun, int verbose) {

  if (verbose) {
    printf("    RLE: a=%08x, lit %6x, bss %6x\n", addr, 4*literalRun, 4*rleRun);
  }

  if (literalRun) {
    int i;
    Put32MixedEndian(fp, addr);
    Put32MixedEndian(fp, 4*literalRun);
    Put32MixedEndian(fp, s->flags);
    Put32MixedEndian(fp, 0);  // Future expansion

    for (i=0; i<literalRun; i++) {
      Put32HeadbangEndian(fp, *d++);
    }
    addr += 4*literalRun;
  }

  if (rleRun) {
    Put32MixedEndian(fp, addr);
    Put32MixedEndian(fp, 4*rleRun);
    Put32MixedEndian(fp, s->flags | FL_BSS);
    Put32MixedEndian(fp, 0);  // Future expansion

    d += rleRun;
    addr += 4*rleRun;
  }
  return addr;
}


void DumpSection(struct Section *s, FILE *fp, int offset, int verbose) {
  int i;
  unsigned int addr = s->addr + offset;
  int rleRun = 0;
  int base = 0;
  
#ifdef USE_BSS_COMPRESSION
  for (i=0; i<sectWords; i++) {
    if (mySectData[i]) {
      if (rleRun >= RLE_RUN_LIMIT_32BIT_WORDS) {
	int literalRun = i-base-rleRun;
	/* Align literalRun to 2 longs = 8 bytes */
	if ((literalRun & 1) && rleRun) {
	  rleRun--;
	  literalRun++;
	}
	/* Align RLE to 2 longs = 8 bytes */
	if (rleRun & 1) {
	  i--;
	  rleRun--;
	}
	//	printf("#0 Purge non-rle %d (%08x) + rle %d\n", i-base-rleRun, d[i], rleRun);
	addr = PurgeSections(s, fp, mySectData+base, addr, literalRun, rleRun, verbose);
	base = i;
      }
      rleRun = 0;
    } else {
      rleRun++;
    }
  }
  //  printf("#1 Purge non-rle %d + rle %d\n", i-base-rleRun, rleRun);
  {
    int literalRun = i-base-rleRun;
    /* Align literalRun to 2 longs = 8 bytes */
    if ((literalRun & 1) && rleRun) {
      rleRun--;
      literalRun++;
    }
    if (rleRun < RLE_RUN_LIMIT_32BIT_WORDS) rleRun = 0;
    addr = PurgeSections(s, fp, mySectData+base, addr, literalRun, rleRun, verbose);
    base = i;
  }
#else
  /* Dump without RLE compression. */
  Put32MixedEndian(fp, s->addr);
  Put32MixedEndian(fp, s->fileSize);
  Put32MixedEndian(fp, s->flags);
  Put32MixedEndian(fp, 0);  // Future expansion

  for (i=0; i<sectWords; i++) {
    Put32HeadbangEndian(fp, mySectData[i]);
  }
  /* Align run to 2 longs = 8 bytes */
  addr = s->addr + ((sectWords + 1) & ~1)*4;
#endif

  currentAddress = addr;
#if 0
  printf("### currentA = %08x\n", currentAddress);
#endif
  
  free(mySectData);
  mySectData = NULL;
}



int HandleRawFile(struct Section *sect, FILE *ofp, int verbose) {
  FILE *fp = fopen(sect->name, "rb");
  int oldInSize = inSize, oldOutSize = outSize;
  int errCode = -1;

  if (sect->addr == 0xFFFFFFFF) {
    sect->addr = currentAddress;
  }
  
  if (verbose) printf("RAW file at %08x, %s\n", sect->addr, sect->name);
  if (!fp) {
    fprintf(stderr, "ERROR! %s not found\n", sect->name);
    goto finally;
  }
  
  fseek(fp, 0, SEEK_END);
  inSize = rawInSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  outSize = 0;

  errCode = 0;

  PrepareSection((inSize+3)/4);
  
  while (inSize) {
    unsigned int w = 0;
    if (inSize) { inSize--; w |= fgetc(fp) << 24; }
    if (inSize) { inSize--; w |= fgetc(fp) << 16; }
    if (inSize) { inSize--; w |= fgetc(fp) <<  8; }
    if (inSize) { inSize--; w |= fgetc(fp) <<  0; }
    AddSection32(w);
  }

#if 0
  printf("AUTOADDRESS %d, CURRENTADDRESS 0x%08x\n",
	 autoAddress, currentAddress);
#endif
  DumpSection(sect, ofp, 0, verbose);
 finally:
  if (fp) {
    fclose(fp);
    fp = NULL;
  }
  if (mySectData) {
    free(mySectData);
    mySectData = NULL;
  }
  rawOutSize = outSize;
  inSize = oldInSize;
  outSize = oldOutSize;

  return errCode;
}



int main(int argc, char **argv) {
  char s[3][STR_SIZE];
  int retVal = EXIT_FAILURE;
  FILE *ifp = NULL, *ofp = NULL;
  struct Section sect[MAX_SECTIONS];
  struct Section rawSect[2][MAX_RAW_FILES] = {0};
  int nSect = 0, nSectName = 0;
  enum Phase phase = phNone;
  int i;
  int verbose = 0;
  char *iName = NULL, *oName = NULL;
  int rawSectors[2] = {0};
  unsigned int flags = FL_READ|FL_WRITE|FL_EXECUTE;
  unsigned int addr = 0;
  unsigned int offset = 0;
  int rawOutput = 0;
  
  for (i=1; i<argc; i++) {
    if (!strcmp(argv[i], "-h")) {
      printf("elf2vri 2025-05-08\n\n"
	     "Usage:\n"
	     "%s [-a addr] [-f flags] [-b bin1 [-b bin2 [...]]] "
	     "[-B bin3 [-B bin4 [...]] [-o offset] "
	     "i.elf [o.vri]\n\n"
	     "i.elf\tRead from i.elf, if no elf input use \"-\"\n"
	     "o.vri\tWrite to o.vri. If \"-\", turn verbose off and write to stdout\n"
	     "-f fl\tSet flags for next binary file(s), 1=READ, 2=WRITE, 4=EXECUTE\n"
	     "-a addr\tSet address for next binary file(s), turn autoincrement off\n"
	     "+a\tTurn address autoincrement on\n"
	     "-b bin\tRead binary file before ELF file (multiple allowed)\n"
	     "-B bin\tRead binary file after ELF file (multiple allowed)\n"
	     "-s offs\tAdd offs to all ELF addresses\n"
	     "-s a:b\tAdd offset of b-a to move ELF from address a to b\n"
	     "-r|+r\tRaw output mode off/on. Raw output can be "
	     "appended to existing VRI file\n"
	     "-v|+v\tVerbose off/on\n"
	     "-h\tShow this help\n", argv[0]);
      printf("\nNOTE! -a, +a, and -f parameters may be interspersed with "
	     "-b & -B\n");
      retVal = EXIT_SUCCESS;
      goto finally;
    } else if (!strcmp(argv[i], "-a")) {
      if (i >= argc-1) {
	fprintf(stderr, "%s: no address for \"%s\"\n", argv[0], argv[i]);
	goto finally;
      }
      currentAddress = addr = strtol(argv[++i], NULL, 0);
      autoAddress = 0;
    } else if (!strcmp(argv[i], "+a")) {
      autoAddress = 1;
    } else if (!strcmp(argv[i], "-f")) {
      if (i >= argc-1) {
	fprintf(stderr, "%s: no flags for \"%s\"\n", argv[0], argv[i]);
	goto finally;
      }
      flags = strtol(argv[++i], NULL, 0) & ~FL_BSS;
    } else if (!strcmp(argv[i], "-b")) {
      if (i >= argc-1) {
	fprintf(stderr, "%s: no file name for \"%s\"\n", argv[0], argv[i]);
	goto finally;
      }
      if (rawSectors[0] >= MAX_RAW_FILES) {
	fprintf(stderr, "%s: only %d files allowed for parmeter \"%s\"\n",
		argv[0], MAX_RAW_FILES, argv[i]);
	goto finally;
      }
      rawSect[0][rawSectors[0]].addr = autoAddress ? -1 : addr;
      rawSect[0][rawSectors[0]].flags = flags;
      rawSect[0][rawSectors[0]].name = argv[++i];
      rawSectors[0]++;
    } else if (!strcmp(argv[i], "-B")) {
      if (i >= argc-1) {
	fprintf(stderr, "%s: no file name for \"%s\"\n", argv[0], argv[i]);
	goto finally;
      }
      if (rawSectors[1] >= MAX_RAW_FILES) {
	fprintf(stderr, "%s: only %d files allowed for parmeter \"%s\"\n",
		argv[0], MAX_RAW_FILES, argv[i]);
	goto finally;
      }
      rawSect[1][rawSectors[1]].addr = autoAddress ? -1 : addr;
      rawSect[1][rawSectors[1]].flags = flags;
      rawSect[1][rawSectors[1]].name = argv[++i];
      rawSectors[1]++;
    } else if (!strcmp(argv[i], "-a")) {
      if (i >= argc-1) {
	fprintf(stderr, "%s: no address for \"%s\"\n", argv[0], argv[i]);
	goto finally;
      }
      addr = strtol(argv[++i], NULL, 0);
    } else if (!strcmp(argv[i], "-f")) {
      if (i >= argc-1) {
	fprintf(stderr, "%s: no flags for \"%s\"\n", argv[0], argv[i]);
	goto finally;
      }
      flags = strtol(argv[++i], NULL, 0) & ~FL_BSS;
    } else if (!strcmp(argv[i], "-s")) {
      char *p;
      if (i >= argc-1) {
	fprintf(stderr, "%s: no offset for \"%s\"\n", argv[0], argv[i]);
	goto finally;
      }
      offset = strtol(argv[++i], NULL, 0);
      if ((p = strchr(argv[i], ':'))) {
	offset = strtol(p+1, NULL, 0) - offset;
      }
    } else if (!strcmp(argv[i], "-r")) {
      rawOutput = 0;
    } else if (!strcmp(argv[i], "+r")) {
      rawOutput = 1;
    } else if (!strcmp(argv[i], "-v")) {
      verbose = 0;
    } else if (!strcmp(argv[i], "+v")) {
      verbose = 1; 
    } else if (!iName) {
      iName = argv[i];
    } else if (!oName) {
      oName = argv[i];
    } else {
      fprintf(stderr, "%s: extraneous parameter \"%s\"\n", argv[0], argv[i]);
      goto finally;
    }
  }

  if (!iName && !rawSectors[0] && !rawSectors[1]) {
    fprintf(stderr, "%s: Nothing to do!\n", argv[0]);
    goto finally;
  }

  if (oName && strcmp(oName, "-")) {
    if (!(ofp = fopen(oName, "wb"))) {
      fprintf(stderr, "%s: Couldn't open %s\n", argv[0], oName);
      goto finally;
    }
  } else {
    ofp = stdout;
    /* Verbose will clash by also printing to stdout, turn it off. */
    verbose = 0;
  }

  if (!rawOutput) fprintf(ofp, "VRI1");
  outSize += 4;

  /* Handle RAW files in front of ELF */
  for (i=0; i<rawSectors[0]; i++) {
    if (HandleRawFile(&rawSect[0][i], ofp, verbose)) {
      fprintf(stderr, "%s: cannot handle raw file \"%s\"\n",
	      argv[0], rawSect[0][i].name);
      goto finally;
    }
    if (verbose) {
      printf("RAW compressed %d bytes to %d "
	     "(%.1f%% compression)\n",
	     rawInSize, rawOutSize,
	     rawInSize ? 100.0*(rawInSize-rawOutSize)/rawInSize : 0.0);
    }
  }


  /* Handle ELF file */
  if (iName && strcmp(iName, "-")) {
    if ( !(ifp = fopen(iName, "rb"))) {
      fprintf(stderr, "%s: Can't open \"%s\"\n", argv[0], s[0]);
      goto finally;
    }
    fseek(ifp, 0, SEEK_END);
    inSize = ftell(ifp);
    fclose(ifp);
    ifp = NULL;

    sprintf(s[0], "readelf %s -l", iName);
    if (verbose) printf("  EX: %s\n", s[0]);
    if (!(ifp = popen(s[0], "r"))) {
      fprintf(stderr, "%s: \"%s\" failed\n", argv[0], s[0]);
      goto finally;
    }
  
    while (fgets(s[0], STR_SIZE-1, ifp)) {
      int n;

      //    printf("phase %d: %s", phase, s[0]);
    
      switch (phase) {
      case phProgramHeaders: {
	unsigned int off, virt, phys, fileSize, memSize, align;
	n = sscanf(s[0], "%s %x %x %x %x %x %s %x",
		   s[1], &off, &virt, &phys, &fileSize, &memSize, s[2], &align);
	if (n == 1 && strstr(s[1], "Type")) {
	  /* Do nothing, wait for loadable data. */
	} else if (n == 8) {
	  if (virt != phys) {
	    fprintf(stderr, "%s: virtual 0x%x != physical 0x%x address\n",
		    argv[0], virt, phys);
	    goto finally;
	  }
#if 0
	  if (memSize < fileSize) {
	    fprintf(stderr, "%s: mem size 0x%x < file size 0x%x\n",
		    argv[0], memSize, fileSize);
	    goto finally;
	  }
#endif
	  sect[nSect].addr = phys;
	  sect[nSect].fileSize = fileSize;
#if 0
	  printf("sect[%x].fileSize = %x\n", nSect, fileSize);
#endif
	  sect[nSect].memSize = memSize;
	  if (strchr(s[2], 'R')) sect[nSect].flags |= FL_READ;
	  if (strchr(s[2], 'W')) sect[nSect].flags |= FL_WRITE;
	  if (strchr(s[2], 'E')) sect[nSect].flags |= FL_EXECUTE;
	  sect[nSect].name = NULL;
	  nSect++;
	} else {
	  phase = phNone;
	}
      }
	break;
      case phSegmentSections: {
	int segSectNo = 0;
	if ((n = sscanf(s[0], "%d %s %s", &segSectNo, s[1], s[2])) > 1) {
	  if (segSectNo == nSectName) {
	    if (n == 2) {
	      sect[nSectName].name = strdup(s[1]);
	      nSectName++;
	    } else if (n == 3) {
	      char *p = strstr(s[0], s[1]);
	      sect[nSectName].name = strdup(p);
	      /* Remove trailing space. */
	      p = sect[nSectName].name+strlen(sect[nSectName].name)-1;
	      while (p != sect[nSectName].name && isspace(*p)) *(--p) = '\0';
	      nSectName++;
	    } else {
	      phase = phNone;
	    }
	  } else {
	    phase = phNone;
	  }
	  break;
	}
      }
      default:
	if (strstr(s[0], "Program Headers:")) {
	  phase = phProgramHeaders;
	} else if (strstr(s[0], "Segment Sections")) {
	  phase = phSegmentSections;
	}
	break;
      }
    }
    fclose(ifp);
    ifp = NULL;

    if (verbose) {
      printf("ELF had %d sections, %d with name\n", nSect, nSectName);
      for (i=0; i<nSect; i++) {
	printf("  Sect %d, addr %08x, file %7x, mem %7x, fl %01x: %s\n",
	       i, (sect[i].addr+offset) % 0xFFFFFFFF, sect[i].fileSize,
	       sect[i].memSize, sect[i].flags, sect[i].name);
      }
    }
#if 0
    if (nSect != nSectName) {
      printf("%s: %d sections of %d has a name\n",
	     argv[0], nSectName, nSect);
      goto finally;
    }
#endif
  }


  for (i=0; i<nSect; i++) {
    if (sect[i].memSize) {
      unsigned int addr = sect[i].addr;
      int bssSize;
      char *nam;

      /* This is a controversial move. Dangerous? Idk, probably not. */
      sect[i].fileSize = (sect[i].fileSize+3)&~3;

      PrepareSection(sect[i].fileSize);
      bssSize = sect[i].memSize - sect[i].fileSize;
      if (verbose) {
	printf("  sect[%d].fileSize = 0x%x, bssSize = 0x%x\n",
	       i, sect[i].fileSize, bssSize);
      }

      nam = strtok(sect[i].name, " \r\n");
      
      while (nam) {
	sprintf(s[0], "readelf %s -x %s", iName, nam);
	if (verbose) printf("  EX: %s\n", s[0]);
	if (!(ifp = popen(s[0], "r"))) {
	  fprintf(stderr, "%s: \"%s\" failed\n", argv[0], s[0]);
	  goto finally;
	}

	while (fgets(s[0], STR_SIZE-1, ifp)) {
	  int n;
	  unsigned int x[5];

	  x[0] = x[1] = x[2] = x[3] = x[4] = 0x12345678;

	  /*
	    Up to 16 rightmost characters of a line is ASCII dump of the
	    hex values. Here we will cut it away.
	   */
	  n = strlen(s[0]);
	  if (n > 16) s[0][n-16] = '\0';

	  if ((n = sscanf(s[0], "%x %x %x %x %x",
			  x+0, x+1, x+2, x+3, x+4)) > 1) {
	    if (addr < x[0]) {
	      unsigned int pad = x[0]-addr;
	      if (verbose) printf("  Padding %x bytes\n", pad);
	      if ((x[0]-addr) & 3) {
		printf("%s: padding %x bytes failed, must be divisable by 4\n",
		       argv[0], pad);
		goto finally;
	      }
	      addr += pad;
	      do {
		AddSection32(0);
	      } while (pad-=4);
	    }
	    if (addr != x[0]) {
	      printf("%s: File %s address 0x%x doesn't match with current "
		     "(non-offset) address %x\n",
		     argv[0], iName, x[0], addr);
	      goto finally;
	    }
	    AddSection32(x[1]);
	    if (n > 2) AddSection32(x[2]);
	    if (n > 3) AddSection32(x[3]);
	    if (n > 4) AddSection32(x[4]);
	    addr += (n-1)*4;
	  }
	}
	nam = strtok(NULL, " \r\n");
      }

      if (verbose) {
	printf("  Section %x data %7x bytes\n", i, addr-sect[i].addr);
      }

      DumpSection(&sect[i], ofp, offset, verbose);

      fclose(ifp);
      ifp = NULL;

      if (sect[i].fileSize != addr-sect[i].addr) {
	fprintf(stderr,
		"ERROR! Wrote 0x%x bytes while fileSize was 0x%x bytes!\n",
	       addr-sect[i].addr, sect[i].fileSize);
	fprintf(stderr,
		"  The file may still work, but this is highly suspicious!\n");
	goto finally;
      }
      if (bssSize > 0) {
	if (verbose) printf("  Section %x bss  %7x bytes\n", i, bssSize);
	Put32MixedEndian(ofp, sect[i].addr+sect[i].fileSize);
	Put32MixedEndian(ofp, bssSize);
	Put32MixedEndian(ofp, sect[i].flags|FL_BSS); // Remove execute?
	Put32MixedEndian(ofp, 0);  // Future expansion
      }
    }
  }

  if (verbose) {
    printf("RLE/BSS/strip compressed %d bytes to %d "
	   "(%.1f%% compression)\n",
	   inSize, outSize, inSize ? 100.0*(inSize-outSize)/inSize : 0.0);
  }

  /* Handle RAW files after ELF */
  for (i=0; i<rawSectors[1]; i++) {
    if (HandleRawFile(&rawSect[1][i], ofp, verbose)) {
      fprintf(stderr, "%s: cannot handle raw file \"%s\"\n",
	      argv[0], rawSect[1][i].name);
      goto finally;
    }
    if (verbose) {
      printf("RAW compressed %d bytes to %d "
	     "(%.1f%% compression)\n",
	     rawInSize, rawOutSize,
	     rawInSize ? 100.0*(rawInSize-rawOutSize)/rawInSize : 0.0);
    }
  }

  retVal = EXIT_SUCCESS;
 finally:
  if (ifp) {
    fclose(ifp);
    ifp = NULL;
  } 
  if (ofp) {
    if (ofp != stdout) fclose(ofp);
    ofp = NULL;
  }
 return retVal;
}


/*

  FULL DOCUMENTATION FOR THE VRI FORMAT:

  "VRI1"
  Section0
  ...
  SectionN-1

  Sections can be either literal run or RLE zero run:

  Section, literal run:
      56 78 12 34	ADDR: Address (mixed-endian, value is 0x12345678)
      56 78 12 34	SIZE: Literal run size in bytes
      00 0? 00 00	FLAGS: FL_READ 1, FL_WRITE 2, FL_EXECUTE 4, no FL_BSS 8
      00 00 00 00	EXTENSION: 0
    n*56 78 12 34	Literal data n=SIZE/4.

  Section, RLE zero run:
      56 78 12 34	ADDR: Address (mixed-endian, value is 0x12345678)
      56 78 12 34	SIZE: Literal run size in bytes
      00 0? 00 00	FLAGS: FL_READ 1, FL_WRITE 2, FL_EXECUTE 4, set FL_BSS 8
      00 00 00 00	EXTENSION: 0

  Note!
  Sections are independent of each other so new sections can be
  appended to an existing image with no issues.

  Note!
  All 32-bit values are encoded in a similar mixed-endian way.

  Note!
  Literal and RLE zero runs are recognized by the FL_BSS bit: if FL_BSS is set
  the section is of type RLE zero run. If FL_BSS is clear then section is a
  literal run.

 */

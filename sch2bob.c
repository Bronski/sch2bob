/**
 * Copyright 2011 Arlen Cuss
 * http://sairyx.org
 * 
 * sch2bob is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * sch2bob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with sch2bob.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef uint8_t TAG_Byte;	/* Type 1 */
typedef uint16_t TAG_Short;	/* Type 2 */
typedef uint32_t TAG_Int;	/* Type 3 */
typedef uint64_t TAG_Long;	/* Type 4 */
typedef float TAG_Float;	/* Type 5 */
typedef double TAG_Double;	/* Type 6 */

/* Type 7 */
typedef struct {
    TAG_Int length;
    TAG_Byte *bytes;
} TAG_Byte_Array;

/* Type 8 */
typedef struct {
    TAG_Short length;
    char *bytes;
} TAG_String;

/* Type 9 */
typedef struct {
    TAG_Byte tagId;
    TAG_Int length;
    void **tags;
} TAG_List;

typedef struct {
    TAG_Byte tagType;
    TAG_String *name;
    void *tag;
} Named_TAG;

/* Type 10 */
typedef struct TAG_Compound {
    /* On-file layout is sequential Named_TAGs until a NUL byte
     * (TAG_End, i.e. TYPE 0). */
    Named_TAG *tag;
    struct TAG_Compound *next;
} TAG_Compound;

int read_ensure(int fd, void *buffer, size_t count) {
    while (count > 0) {
	ssize_t n = read(fd, buffer, count);
	if (n < 0) {
	    fprintf(stderr, "fail in reading %d bytes: %d\n", (int)count, (int)n);
	    return n;
	}
	count -= n;
	buffer = ((uint8_t *)buffer) + n;
    }
    return 0;
}

int write_ensure(int fd, const void *buffer, size_t count) {
    while (count > 0) {
	ssize_t n = write(fd, buffer, count);
	if (n < 0) {
	    fprintf(stderr, "fail in writing %d bytes: %d\n", (int)count, (int)n);
	    return n;
	}
	count -= n;
	buffer = ((uint8_t *)buffer) + n;
    }
    return 0;
}

int write_ensure_str(int fd, const char *str) {
    return write_ensure(fd, str, strlen(str));
}

TAG_Byte TAG_Byte_read(int fd) {
    TAG_Byte r;
    read_ensure(fd, &r, sizeof(r));
    return r;
}

TAG_Short TAG_Short_read(int fd) {
    TAG_Short r;
    read_ensure(fd, &r, sizeof(r));
    return be16toh(r);
}

TAG_Int TAG_Int_read(int fd) {
    TAG_Int r;
    read_ensure(fd, &r, sizeof(r));
    return be32toh(r);
}

TAG_Long TAG_Long_read(int fd) {
    TAG_Long r;
    read_ensure(fd, &r, sizeof(r));
    return be64toh(r);
}

TAG_Float TAG_Float_read(int fd) {
    TAG_Float r;
    read_ensure(fd, &r, sizeof(r));
    return r;
}

TAG_Double TAG_Double_read(int fd) {
    TAG_Double r;
    read_ensure(fd, &r, sizeof(r));
    return r;
}

TAG_String *TAG_String_read(int fd) {
    TAG_String *r = malloc(sizeof(*r));
    r->length = TAG_Short_read(fd);
    r->bytes = malloc(r->length + 1);
    read_ensure(fd, r->bytes, r->length);
    r->bytes[r->length] = 0;
    return r;
}

void TAG_String_free(TAG_String *r) {
    if (!r) return;

    free(r->bytes);
    free(r);
}

TAG_Byte_Array *TAG_Byte_Array_read(int fd) {
    TAG_Byte_Array *r = malloc(sizeof(*r));
    r->length = TAG_Int_read(fd);
    r->bytes = malloc(r->length);
    read_ensure(fd, r->bytes, r->length);
    return r;
}

void TAG_Byte_Array_free(TAG_Byte_Array *r) {
    if (!r) return;

    free(r->bytes);
    free(r);
}

void *TAG_read(int fd, TAG_Byte tagType);

TAG_List *TAG_List_read(int fd) {
    TAG_List *r = malloc(sizeof(*r));
    r->tagId = TAG_Byte_read(fd);
    r->length = TAG_Int_read(fd);
    r->tags = malloc(sizeof(void *) * r->length);
    for (int i = 0; i < r->length; ++i)
	r->tags[i] = TAG_read(fd, r->tagId);
    return r;
}

void TAG_free(void *tag, TAG_Byte tagType);

void TAG_List_free(TAG_List *r) {
    if (!r) return;

    for (int i = 0; i < r->length; ++i)
	TAG_free(r->tags[i], r->tagId);
    free(r->tags);
    free(r);
}

Named_TAG *Named_TAG_read(int fd);

TAG_Compound *TAG_Compound_read(int fd) {
    Named_TAG *tag = Named_TAG_read(fd);

    if (tag->tagType == 0) {
	free(tag);
	return NULL;
    }

    TAG_Compound *r = malloc(sizeof(*r));
    r->tag = tag;
    r->next = TAG_Compound_read(fd);
    return r;
}

void Named_TAG_free(Named_TAG *tag);

void TAG_Compound_free(TAG_Compound *r) {
    if (!r) return;

    Named_TAG_free(r->tag);
    TAG_Compound *next = r->next;
    free(r);
    TAG_Compound_free(next);
}

void *TAG_read(int fd, TAG_Byte tagType) {
    switch (tagType) {
    case 1: {
	TAG_Byte *r = malloc(sizeof(*r));
	*r = TAG_Byte_read(fd);
	return r;
    }
    case 2: {
	TAG_Short *r = malloc(sizeof(*r));
	*r = TAG_Short_read(fd);
	return r;
    }
    case 3: {
	TAG_Int *r = malloc(sizeof(*r));
	*r = TAG_Int_read(fd);
	return r;
    }
    case 4: {
	TAG_Long *r = malloc(sizeof(*r));
	*r = TAG_Long_read(fd);
	return r;
    }
    case 5: {
	TAG_Float *r = malloc(sizeof(*r));
	*r = TAG_Float_read(fd);
	return r;
    }
    case 6: {
	TAG_Double *r = malloc(sizeof(*r));
	*r = TAG_Double_read(fd);
	return r;
    }
    case 7:
	return TAG_Byte_Array_read(fd);
    case 8:
	return TAG_String_read(fd);
    case 9:
	return TAG_List_read(fd);
    case 10:
	return TAG_Compound_read(fd);
    default:
	fprintf(stderr, "unknown tag type %d\n", tagType);
	return NULL;
    }
}

void TAG_free(void *tag, TAG_Byte tagType) {
    switch (tagType) {
    case 0:
	return;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
	free(tag);
	return;
    case 7:
	TAG_Byte_Array_free(tag);
	return;
    case 8:
	TAG_String_free(tag);
	return;
    case 9:
	TAG_List_free(tag);
	return;
    case 10:
	TAG_Compound_free(tag);
	return;
    default:
	fprintf(stderr, "unknown tag type to free %d\n", tagType);
    }
}

Named_TAG *Named_TAG_read(int fd) {
    Named_TAG *r = malloc(sizeof(*r));
    r->tagType = TAG_Byte_read(fd);
    if (r->tagType == 0) {
	r->name = NULL;
	r->tag = NULL;
	return r;
    }

    r->name = TAG_String_read(fd);
    r->tag = TAG_read(fd, r->tagType);

    return r;
}

void Named_TAG_free(Named_TAG *tag) {
    TAG_String_free(tag->name);
    TAG_free(tag->tag, tag->tagType);

    free(tag);
}

Named_TAG *Named_TAG_find(Named_TAG *base, const char *name) {
    if (base->name && strcmp(base->name->bytes, name) == 0)
	return base;

    if (base->tagType != 10)
	return NULL;

    TAG_Compound *comp = base->tag;

    while (comp) {
	Named_TAG *r = Named_TAG_find(comp->tag, name);
	if (r)
	    return r;
	
	comp = comp->next;
    }

    return NULL;
}

int Named_TAG_number(Named_TAG *n) {
    switch (n->tagType) {
    case 1:
	return (int)*((TAG_Byte *)n->tag);
    case 2:
	return (int)*((TAG_Short *)n->tag);
    case 3:
	return (int)*((TAG_Int *)n->tag);
    case 4:
	return (int)*((TAG_Long *)n->tag);
    default:
	fprintf(stderr, "'%s' is not a numeric type (%d)\n", n->name->bytes, n->tagType);
	return 0;
    }
}

void convert_file(const char *filename) {
    char *outfile = malloc(strlen(filename) + 5);
    strcpy(outfile, filename);
    char *dot = strchr(outfile, '.');
    if (!dot)
	dot = outfile + strlen(outfile);
    strcpy(dot, ".bo2");

    int fd = open(outfile, 0);
    if (fd != -1) {
	close(fd);
	free(outfile);

	printf("Skipping: %s\n", filename);
	return;
    }

    printf("Processing: %s\n", filename);

    fd = open(filename, 0);
    Named_TAG *tag = Named_TAG_read(fd);
    close(fd);

    Named_TAG *theight     = Named_TAG_find(tag, "Height"),
              *tlength     = Named_TAG_find(tag, "Length"),
              *twidth      = Named_TAG_find(tag, "Width"),
	      *tblocks     = Named_TAG_find(tag, "Blocks"),
              *tblock_data = Named_TAG_find(tag, "Data");

    if (!theight || !tlength || !twidth || !tblocks || !tblock_data) {
	fprintf(stderr, "%s missing a vital tag\n", filename);
	free(outfile);
	return;
    }

    int height = Named_TAG_number(theight),
        length = Named_TAG_number(tlength),
        width  = Named_TAG_number(twidth);

    if (!height || !width || !length) {
	fprintf(stderr, "%s's dimensions are messed up\n", filename);
	free(outfile);
	return;
    }

    if (tblocks->tagType != 7 || tblock_data->tagType != 7) {
	fprintf(stderr, "%s blocks or block data incorrect type\n", filename);
	free(outfile);
	return;
    }

    TAG_Byte_Array *blocks     = tblocks->tag,
                   *block_data = tblock_data->tag;

    if (height * length * width != blocks->length
	|| blocks->length != block_data->length) {
	fprintf(stderr, "%s has inconsistent data\n", filename);
	free(outfile);
	return;
    }
    
    FILE *f = fopen(outfile, "w");

    fprintf(f, "[META]\n");
    fprintf(f, "version=2.0\n");
    fprintf(f, "spawnElevationMin=0\n");
    fprintf(f, "spawnElevationMax=128\n");
    fprintf(f, "rarity=100\n");
    fprintf(f, "collisionPercentage=2\n");
    fprintf(f, "[DATA]\n");

    int index = 0;
    for (int y = 0; y < height; ++y)
	for (int z = 0; z < length; ++z)
	    for (int x = 0; x < width; ++x) {
		TAG_Byte b = blocks->bytes[index];
		if (b != 0) {
		    TAG_Byte d = block_data->bytes[index];
		    fprintf(f, "%d,%d,%d:%d.%d\n", x, z, y, b, d);
		}

		++index;
	    }

    fclose(f);

    Named_TAG_free(tag);
    free(outfile);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
	convert_file(argv[i]);
    }

    return 0;
}

/* vim: set sw=4 ts=8 noet: */

// Definitions for the various Object Module Formats
#include "stdio.h"
#include "link.h"

#define THEADR 0x80
#define MODEND 0x8A
#define EXTDEF 0x8C
#define PUBDEF 0x90
#define LNAMES 0x96
#define SEGDEF 0x98
#define FIXUPP 0x9C
#define LEDATA 0xA0

char twotabs[] = "\n        ";
extern char *line;

do_record(byte recType, uint length, uint fd) {
    prnhexch(recType, stdout);
    fputc(' ', stdout);
    prnhexch(length / 256, stdout);
    prnhexch(length & 0x00ff, stdout);
    fputc(' ', stdout);
    switch (recType) {
        case THEADR:
            do_theadr(length, fd);
            break;
        case MODEND:
            do_modend(length, fd);
            break;
        case EXTDEF:
            do_extdef(length, fd);
            break;
        case PUBDEF:
            do_pubdef(length, fd);
            break;
        case LNAMES:
            do_lnames(length, fd);
            break;
        case SEGDEF:
            do_segdef(length, fd);
            break;
        case FIXUPP:
            do_fixupp(length, fd);
            break;
        case LEDATA:
            do_ledata(length, fd);
            break;
        default:
            fputs("UNKNOWN ", stdout);
            fgetc(stdin);
            clearrecord(length, fd);
            break;
    }
    fputc(NEWLINE, stdout);
}

clearrecord(uint length, uint fd) {
    while (length-- > 0) {
        byte ch;
        ch = read_u8(fd);
        prnhexch(ch, stdout);
    }
}

// 80H THEADR Translator Header Record
// The THEADR record contains the name of the object module.  This name
// identifies an object module within an object library or in messages produced
// by the linker. The name string indicates the full path and filename of the
// file that contained the source code for the module.
// This record, or an LHEADR record must occur as the first object record.
// More than one header record is allowed (as a result of an object bind, or if
// the source arose from multiple files as a result of include processing).
// 82H is handled identically, but indicates the name of a module within a
// library file, which has an internal organization different from that of an
// object module.
do_theadr(uint length, uint fd) {
    fputs("THEADR ", stdout);
    read_strp(line, fd);
    fputs(line, stdout);
    read_u8(fd); // checksum. assume correct.
    return;
}

// 8AH MODEND Module End Record
// The MODEND record denotes the end of an object module. It also indicates
// whether the object module contains the main routine in a program, and it
// can optionally contain a reference to a program's entry point.
do_modend(uint length, uint fd) {
    byte moduletype;
    fputs("MODEND ", stdout);
    moduletype = read_u8(fd); // format is MS0....1
    if (moduletype & 0x80) {
        fputs("Main ", stdout);
        abort(1);
    }
    if (moduletype & 0x40) {
        fputs("Start ", stdout);
        abort(1);
    }
    read_u8(fd); // checksum. assume correct.
    return;
}

// 8CH EXTDEF External Names Definition Record
// The EXTDEF record contains a list of symbolic external references—that is,
// references to symbols defined in other object modules. The linker resolves
// external references by matching the symbols declared in EXTDEF records
// with symbols declared in PUBDEF records.
do_extdef(uint length, uint fd) {
    byte deftype;
    fputs("EXTDEF ", stdout);
    while (length > 1) {
        length -= read_strpre(line, fd) + 1;
        fputs(line, stdout);
        deftype = read_u8(fd);
        if (deftype != 0) {
            fputs("Error: Type is not 0. ", stdout);
            abort(1);
        }
        fputs(", ", stdout);
    }
    read_u8(fd); // checksum. assume correct.
    return;
}

// 90H PUBDEF Public Names Definition Record
// The PUBDEF record contains a list of public names.  It makes items defined
// in this object module available to satisfy external references in other
// modules with which it is bound or linked. The symbols are also available
// for export if so indicated in an EXPDEF comment record.
do_pubdef(uint length, uint fd) {
    byte basegroup, basesegment, typeindex;
    uint puboffset;
    fputs("PUBDEF ", stdout);
    // BaseGroup and BaseSegment fields contain indexes specifying previously
    // defined SEGDEF and GRPDEF records.  The group index may be 0, meaning
    // that no group is associated with this PUBDEF record.
    // BaseFrame field is present only if BaseSegment field is 0, but the
    // contents of BaseFrame field are ignored.
    // BaseSegment idx is normally nonzero and no BaseFrame field is present.
    basegroup = read_u8(fd);
    fputs("BaseGroup=", stdout);
    prnhexch(basegroup, stdout);
    if (basegroup != 0) {
        fputs(" Error: Must be 0.", stdout);
        abort(1);
    }
    basesegment = read_u8(fd);
    fputs(" BaseSegment=", stdout);
    prnhexch(basesegment, stdout);
    if (basesegment == 0) {
        fputs(" Error: Must be nonzero.", stdout);
        abort(1);
    }
    length -= 2;
    fputs(twotabs, stdout);
    while (length > 1) {
        length -= read_strpre(line, fd) + 3;
        fputs(line, stdout);
        puboffset = read_u16(fd);
        fputc('@', stdout);
        prnhexint(puboffset, stdout);
        typeindex = read_u8(fd);
        if (typeindex != 0) {
            fputs("Error: Type is not 0. ", stdout);
            abort(1);
        }
        fputs(", ", stdout);
    }
    read_u8(fd); // checksum. assume correct.
    return;
}

// 96H LNAMES List of Names Record
// The LNAMES record is a list of names that can be referenced by subsequent
// SEGDEF and GRPDEF records in the object module. The names are ordered by
// occurrence and referenced by index from subsequent records.  More than one
// LNAMES record may appear.  The names themselves are used as segment, class,
// group, overlay, and selector names.

do_lnames(uint length, uint fd) {
    fputs("LNAMES ", stdout);
    while (length > 1) {
        length -= read_strpre(line, fd);
        fputc('"', stdout);
        fputs(line, stdout);
        fputs("\" ", stdout);
    }
    read_u8(fd); // checksum. assume correct.
    return;
}

// 98H SEGDEF Segment Definition Record
// The SEGDEF record describes a logical segment in an object module. It
// defines the segment's name, length, and alignment, and the way the segment
// can be combined with other logical segments at bind, link, or load time.
// Object records that follow a SEGDEF record can refer to it to identify a
// particular segment.  The SEGDEF records are ordered by occurrence, and are
// referenced by segment indexes (starting from 1) in subsequent records.
do_segdef(uint length, uint fd) {
    byte segattr, segname, classname, overlayname;
    uint seglength;
    fputs("SEGDEF ", stdout);
    // segment attribute
    segattr = read_u8(fd);
    if ((segattr & 0xe0) != 0x60) {
        fputs("Unknown segment attribute field (must be 0x03).", stdout);
        abort(1);
    }
    if ((segattr & 0x1c) != 0x08) {
        fputs("Unknown segment acombination (must be 0x02).", stdout);
        abort(1);
    }
    if ((segattr & 0x02) != 0x00) {
        fputs("Attribute may not be big (flag 0x02).", stdout);
    }
    if ((segattr & 0x01) != 0x00) {
        fputs("Attribute must be 16-bit addressing (flag 0x01).", stdout);
    }
    // segment length
    seglength = read_u16(fd);
    fputs("Length=", stdout);
    prnhexint(seglength, stdout);
    // name index
    segname = read_u8(fd);
    fputs(" Name=", stdout);
    prnhexch(segname, stdout);
    // class name
    classname = read_u8(fd);
    fputs(" Class=", stdout);
    prnhexch(classname, stdout);
    // overlay name
    overlayname = read_u8(fd);
    fputs(" Overlay=", stdout);
    prnhexch(overlayname, stdout);
    read_u8(fd); // checksum. assume correct.
    return;
}

// 9CH FIXUPP Fixup Record
// The FIXUPP record contains information that allows the linker to resolve
// (fix up) and eventually relocate references between object modules. FIXUPP
// records describe the LOCATION of each address value to be fixed up, the
// TARGET address to which the fixup refers, and the FRAME relative to which
// the address computation is performed.
// Each subrecord in a FIXUPP object record either defines a thread for
// subsequent use, or refers to a data location in the nearest previous LEDATA
// or LIDATA record. The high-order bit of the subrecord determines the
// subrecord type: if the high-order bit is 0, the subrecord is a THREAD
// subrecord; if the high-order bit is 1, the subrecord is a FIXUP subrecord.
// Subrecords of different types can be mixed within one object record.
// Information that determines how to resolve a reference can be specified
// explicitly in a FIXUP subrecord, or it can be specified within a FIXUP
// subrecord by a reference to a previous THREAD subrecord. A THREAD subrecord
// describes only the method to be used by the linker to refer to a particular
// target or frame. Because the same THREAD subrecord can be referenced in
// several subsequent FIXUP subrecords, a FIXUPP object record that uses THREAD
// subrecords may be smaller than one in which THREAD subrecords are not used.
do_fixupp(uint length, uint fd) {
    uint dataOffset, targetOffset;
    byte fixup, fixdata, frame, target;
    byte relativeMode; // 1 == segment relative, 0 == self relative.
    byte location; 
    fputs("FIXUPP ", stdout);
    while (length > 1) {
        fputs(twotabs, stdout);
        length -= 3;
        fixup = read_u8(fd);
        fputs("Fixup=", stdout);
        prnhexch(fixup, stdout);
        if ((fixup & 0x80) == 0) {
            fputs("Error: must be fixup, not thread.", stdout);
            abort(1);
        }
        relativeMode = (fixup & 0x40) != 0;
        location = (fixup & 0x3c) >> 2;
        dataOffset = read_u8(fd) + ((fixup & 0x03) << 8);
        fixdata = read_u8(fd); // Format is FfffTPtt
        fputc(' ', stdout);
        prnhexch(fixdata, stdout);
        if ((fixdata & 0x80) != 0) {
            frame = (fixdata & 0x70) >> 4;
        }
        else {
            frame = read_u8(fd);
            length -= 1;
            fputc(' ', stdout);
            prnhexch(frame, stdout);
        }
        if ((fixdata & 0x08) != 0) {
            fputs(" Error: target threads not handled (", stdout);
            prnhexch(fixdata, stdout);
            fputc(')', stdout);
            abort(1);
        }
        else {
            target = read_u8(fd);
            length -= 1;
            fputc(' ', stdout);
            prnhexch(target, stdout);
        }
        if ((fixdata & 0x04) == 0) {
            targetOffset = read_u16(fd);
            length -= 2;
            fputc(' ', stdout);
            prnhexint(targetOffset, stdout);
        }
    }
    read_u8(fd); // checksum. assume correct.
    return;
}

// A0H  LEDATA Logical Enumerated Data Record
do_ledata(uint length, uint fd) {
    byte segindex;
    uint dataoffset;
    fputs("LEDATA ", stdout);
    // Segment Index
    segindex = read_u8(fd);
    fputs("SegIndex=", stdout);
    prnhexch(segindex, stdout);
    // Data Offset
    dataoffset = read_u16(fd);
    fputs(" DataOffset=", stdout);
    prnhexint(dataoffset, stdout);
    // Data bytes
    fputs(twotabs, stdout);
    length -= 4;
    clearrecord(length, fd);
    read_u8(fd); // checksum. assume correct.
    return;
}
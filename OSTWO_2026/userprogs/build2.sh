#!/bin/sh
# Build hello2.exe - a genuine OS/2 LX executable - with Open Watcom v2,
# then regenerate the embedded kernel header.
#
# Set WATCOM_BIN to your Open Watcom binl64 directory. The toolchain is
# the Linux x64 build of https://github.com/open-watcom/open-watcom-v2
# (release asset open-watcom-2_0-c-linux-x64; it is a self-extracting
# zip: `unzip open-watcom-2_0-c-linux-x64 -d watcom`).
set -e
WATCOM_BIN=${WATCOM_BIN:-/tmp/claude-1000/-home-franco-OSTwo-OSTWO-2026/84a0a3f1-d641-47f3-b590-0a6891c18790/scratchpad/watcom/binl64}

"$WATCOM_BIN/wcc386" -bt=os2 -s -zl -oxs -fo=hello2.o hello2.c
"$WATCOM_BIN/wlink" @hello2.lnk
"$WATCOM_BIN/wcc386" -bt=os2 -s -zl -oxs -fo=hello3.o hello3.c
"$WATCOM_BIN/wlink" @hello3.lnk

# hello4: full C runtime (clib startup + printf); needs the Watcom
# headers and OS/2 libraries from the toolchain tree
WATCOM="$(dirname "$WATCOM_BIN")" INCLUDE="$(dirname "$WATCOM_BIN")/h" \
    PATH="$WATCOM_BIN:$PATH" \
    "$WATCOM_BIN/wcl386" -bt=os2 -l=os2v2 -s -fe=hello4.exe hello4.c
WATCOM="$(dirname "$WATCOM_BIN")" INCLUDE="$(dirname "$WATCOM_BIN")/h" \
    PATH="$WATCOM_BIN:$PATH" \
    "$WATCOM_BIN/wcl386" -bt=os2 -l=os2v2 -s -fe=hello5.exe hello5.c

python3 -c "
for exe, sym in [('hello2.exe', 'hello2_bin'), ('hello3.exe', 'hello3_bin'), ('hello4.exe', 'hello4_bin'), ('hello5.exe', 'hello5_bin')]:
    img = open(exe, 'rb').read()
    with open('../' + sym + '.h', 'w') as f:
        f.write('// Generated from userprogs/' + exe + ' (Open Watcom wcc386 + wlink, FORMAT os2 lx)\n')
        f.write('static const unsigned char ' + sym + '[] = {\n')
        for i in range(0, len(img), 12):
            f.write('    ' + ', '.join('0x%02x' % b for b in img[i:i+12]) + ',\n')
        f.write('};\n')
        f.write('static const unsigned int ' + sym + '_len = ' + str(len(img)) + ';\n')
    print(exe + ': ' + str(len(img)) + ' bytes embedded')
"

**NOTE**

This tool was written between and Oct. 26, 2003 and Dec. 04, 2003.

**DESCRIPTION**

Many hardware MP3 players have problem playing Stereo MP3 files encoded with
LAME >= 3.88b. Those players cannot properly decode MP3 frames which have
different block types between channels (right channel has short blocks, and left
channel has long block or vice versa). This is manifested as very short, high
frequency sound in place of those frames, and sometimes, as short silence.
Joint Stereo encoded MP3 files with same LAME wersion, have same block types in
every frame, so they are played correctly on same players.

*THIS IS NOT LAME BUG OR NONCONFORMANCE TO STANDARDS.*

This is hardware decoder incompatibility and is responsibility of hardware
manufacturers to correct this mistake in future versions of their players.

LAME developers are familiar with this problem, and since LAME version 3.94b,
Stereo MP3 files have same block types in every frame (like Joint Stereo).

Also, I noticed that MP3 files encoded with some older encoders have different
block types between channels, but they are played correctly, so they do not need
to be fixed.

This program is intended for people who bought this kind of hardware player, and
have very large number of Stereo MP3 files encoded with LAME. This was case with
me. I mistakenly thought that quality of Stereo MP3 files is better then quality
of Joint Stereo MP3 files. Joint Stereo is always better.

Re-encoding those files would not be a good idea, because it would degrade their
quality a lot. Much better idea would be to reencode only those frames that
mentioned players have problem with.

This program does exactly that. It first decodes a file, then encodes it with
same bitrate, but it does it so, that new MP3 file has same block types in every
frame. Then it replaces "erroneous" frames in original file with frames from new
file.

MBinfo only gives information weather MP3 files should be corrected or not.

MBfixer fixes files.

**COMPILATION**

You need to have LAME installed on your computer.

Compile MBinfo by typing:

    gcc -o MBinfo MBinfo.c frame.c

Compile MBfixer by typing:

    gcc -lmp3lame -lm -o MBfixer MBfixer.c frame.c

Then copy MBinfo and MBfixer to /usr/bin or /usr/local/bin.

**USAGE**

    MBinfo <list of files>
    MBfixer <list of files>

**LICENCE**

There's no licence on this code or program. You are free to do with it what you
like.

## DESCRIPTION

ls-fuse mounts output of 'ls -lR', 'ls -lRZ' or 'ls -l' as a pseudo filesystem.
Output of ftp clients' ls command can be mounted as well.

Purpose of ls-fuse project is similar to [lsfs project][1] or lslR plugin for
midnight commander. But the main goal was implementation of a fast native tool
with SELinux extended attributes support.

lf-fuse features:

* Native mounting tool: standard unix tools such as find(1) can be used
* Easy to use: no additional scripts or stages of preparation
* STDIN support: allows to combine with other unix tools (see EXAMPLE 2)
* Multiple input files support: allows to merge several ls-lR files into single
  directory (see EXAMPLE 3)
* SELinux extended attributes support (broken at the moment, see KNOWN ISSUES)

ls-fuse supports output of ls that was run with the next options:

* -l (mandatory)
* -a (optional)
* -R (optional)
* -s (optional)
* -Z (on systems with SELinux suport, optional)

All regular files on the pseudo filesystem are readable. ls-fuse returns
some information about file when reading.

[1]: http://lsfs.sourceforge.net

## BUILDING FROM SOURCES

Obtain the latest sources from git repo:

	git clone git://github.com/pasis/ls-fuse.git ls-fuse

Also you can get the latest stable tarball at [sourceforge page][2].

If you got sources from git repo you need to run autogen.sh script at first.
It generates configure script. For stable tarballs you already have the
configure script and don't have to generate it. To build ls-fuse run the
following commands:

	./autogen.sh
	./configure
	make

[2]: https://sourceforge.net/projects/lsfuse

## ANDROID

ls-fuse works on Android as native tool. Tested with [fuse-android][3].
Note, ls-fuse requires access to /dev/fuse.

[3]: https://github.com/seth-hg/fuse-android

## INSTALLING

Gentoo users can install ls-fuse package from 'stuff' overlay:

	layman -a stuff
	emerge ls-fuse

Packages for RPM-based distributions can be found at [sourceforge page][2].

For other Linux distributions or operating systems just run as root (after
building from sources of course):

	make install

Note, you don't have to install ls-fuse to use it. You can use executable file
ls-fuse as standalone program.

## EXAMPLE

After building binary you can try it out:

	ls --color=never -lR > test.txt
	mkdir mnt
	./ls-fuse test.txt mnt

After that mnt/ will contain (I hope) files described in test.txt. The
following command will unmount fs:

	fusermount -u mnt

## EXAMPLE 2 (STDIN SUPPORT)

ls-fuse supports reading from standard input stream:

	mkdir ~/mnt
	ls --color=never -lR | ls-fuse ~/mnt

or

	bzip2 -d -c ls-lR.bz2 | ls-fuse ~/mnt

## EXAMPLE 3 (MULTIPLE FILES SUPPORT)

ls-fuse allows to merge a set of ls-lR files to a single directory:

	ls-fuse 1.ls-lR 2.ls-lR 3.ls-lR ~/mnt

Any FUSE options must be placed after the set of files, for example:

	ls-fuse 1.ls-lR 2.ls-lR 3.ls-lR -o ro ~/mnt

Option '-o ro' says FUSE to mount filesystem as read-only.

## KNOWN ISSUES

* getxattr for security.selinux extended attribute doesn't pass to ls-fuse.
  Instead, genfscon rule is used. (Tested on Fedora 17).
* Collisions while mounting several ls-lR files ain't handled for now. If this
  happens you will see several files with the same name. But it's not a
  disaster :)

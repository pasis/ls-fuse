## DESCRIPTION

ls-fuse mounts output of 'ls -lR', 'ls -lRZ' or 'ls -l' as a pseudo filesystem.
Output of ftp clients' ls command can be mounted as well.

Purpose of ls-fuse project is similar to [lsfs project][1] or lslR plugin for
midnight commander. But the main goal was implementation of tool with SELinux
extended attributes support.

lf-fuse features:

* Native mounting tool: standard unix tools such as find(1) can be used
* Easy to use: no additional scripts or stages of preparation
* STDIN support: allows to combine with other unix tools (see EXAMPLE 2)
* Multiple input files support: allows to merge several ls-lR files into single
  directory (not implemented yet)
* SELinux extended attributes support (broken at the moment, see KNOWN ISSUES)

Supported output of ls with options:

* -l (mandatory)
* -a (optional)
* -R (optional)
* -s (optional)
* -Z (on systems with SELinux suport, optional)

[1]: http://lsfs.sourceforge.net

## BUILDING FROM SOURCES

Obtain the latest sources from git repo:

	git clone git://github.com/pasis/ls-fuse.git ls-fuse

Also you can get the latest stable tarball at [sourceforge page][2].

If you got sources from git repo you need to run bootstrap.sh script at first.
It generates configure script. For stable tarballs you already have the
configure script and don't have to generate it. To build ls-fuse run the
following commands:

	./bootstrap.sh
	./configure
	make

[2]: https://sourceforge.net/projects/lsfuse

## INSTALLING

Gentoo users can install ls-fuse package from 'stuff' overlay:

	layman -a stuff
	emerge ls-fuse

For other Linux distributions or operating systems just run as root (after
building from sources of course):

	make install

Note, you don't have to install ls-fuse to use it. You can you executable file
ls-fuse as standalone program.

## EXAMPLE

After building binary you can try it out:

	ls --color=never -lR > test.txt
	mkdir mnt
	./ls-fuse test.txt mnt

After that mnt/ will contain (I hope) files described in test.txt. The
following command will unmount fs:

	fusermount -u mnt

## EXAMPLE 2

ls-fuse supports reading from stdin:

	mkdir ~/mnt
	ls --color=never -lR | ls-fuse ~/mnt

or

	bzip2 -d -c ls-lR.bz2 | ls-fuse ~/mnt


## KNOWN ISSUES

* getxattr for security.selinux extended attribute doesn't pass to ls-fuse.
  Instead, genfscon rule is used. (Tested on Fedora 17).

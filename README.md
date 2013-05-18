## DESCRIPTION

ls-fuse mounts output of 'ls -lR', 'ls -lRZ' or 'ls -l' as a pseudo filesystem.
Output of ftp clients' ls command can be mounted as well.

Supported output of ls with options:

* -a (optional)
* -l (mandatory)
* -R (optional)
* -s (optional)
* -Z (on systems with SELinux suport, optional)

## BUILDING

lf-fuse uses autotools. To build project run the following commands:

	./bootstrap.sh
	./configure
	make

## INSTALLING

Gentoo users can install ls-fuse package from 'stuff' overlay:

	layman -a stuff
	echo "=sys-fs/ls-fuse-9999 **" >> /etc/portage/package.keywords
	emerge ls-fuse

Or after building from sources just run

	make install

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

## KNOWN ISSUES

* getxattr for security.selinux extended attribute doesn't pass to ls-fuse.
  Instead, genfscon rule is used. (Tested on Fedora 17).

## For future

* ls-fuse will be adapted to work under other unix-like systems (macos, freebsd)
* multiple input files support (entry of the files will be combine to a single
  filesystem)

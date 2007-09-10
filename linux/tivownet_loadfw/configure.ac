AC_INIT([tivownet_loadfw], [0.1], [stephenws@users.sourceforge.net])

AC_MSG_NOTICE([$PACKAGE_NAME-$PACKAGE_VERSION  Firmware downloader for the TiVo USB Wireless Adapter.])

AM_INIT_AUTOMAKE([-Wall -Werror gnu])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_CHECK_LIB([usb], [usb_control_msg],,[AC_MSG_ERROR([libusb not found])] )

AC_HEADER_STDC
AC_CHECK_HEADERS([usb.h])

LICENSE=GPL
AC_SUBST(LICENSE)

AC_OUTPUT


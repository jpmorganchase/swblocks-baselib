# project root directory
TOPDIR	:= $(dir $(lastword $(MAKEFILE_LIST)))
MKDIR	:= $(TOPDIR)projects/make

# primary rules, variables, and targets
include $(MKDIR)/common.mk


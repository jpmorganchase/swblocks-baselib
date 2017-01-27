GIT_COMMIT_MESSAGE = "increment plugin build id"
BRANCH ?= master
GITREV := $(shell $(GIT) rev-parse --short HEAD)

ifneq ($(MAKECMDGOALS),clean)
  $(info $(HR))
  $(info Git repository detected... $(shell $(GIT) --version))
  $(info $(HR))
endif

define INCREMENT_BUILD_ID_FILE
	awk '/^#define *BL_PLUGINS_BUILD_ID/ { $$3++ } { print }' \
	  $(BUILD_ID_FILE) > $(BUILD_ID_FILE).tmp && \
	mv -f $(BUILD_ID_FILE).tmp $(BUILD_ID_FILE) && \
	awk '/^#define *BL_PLUGINS_RELEASE_DATE/ { $$3=$(DATE) } { print }' \
	  $(BUILD_ID_FILE) > $(BUILD_ID_FILE).tmp && \
	mv -f $(BUILD_ID_FILE).tmp $(BUILD_ID_FILE)
endef

increment-build-id:
ifneq ($(BRANCH),master)
	$(GIT) checkout master
endif
	$(call INCREMENT_BUILD_ID_FILE)
	$(GIT) commit --message=$(GIT_COMMIT_MESSAGE) $(BUILD_ID_FILE) && \
	$(GIT) push origin master
ifneq ($(BRANCH),master)
	$(GIT) checkout $(BRANCH)
	$(GIT) checkout origin/master -- $(BUILD_ID_FILE)
	$(GIT) commit --message=$(GIT_COMMIT_MESSAGE) $(BUILD_ID_FILE) && \
	$(GIT) push origin $(BRANCH)
endif

.PHONY: increment-build-id

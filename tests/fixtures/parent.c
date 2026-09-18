extern int forge_fixture_child(void);

int forge_fixture_parent(void) { return forge_fixture_child(); }

# Questions to ask while implementing libmapper functionalities

## Devices

1. Difference between dev and dev->loc ???

2. Significance of local device, why is it important?

3. How does polymorphism work in C? Or does it?

## Objects

1. To implement hierarchial structures, can we add a mpr_list of mpr_obj as an optional attribute of the mpr_dev struct?

2. Should there be some way to copy mpr_objs for passing both a child and a parent, add to list and then copy attributes from passed child to new list item???

3. Would it be better to implement a new "child node" type of MPR object? (Issue I see here is that it would allow only a depth of 1 child from a parent, no grandchildren or further.)

4. Would it be better to fake the hierarchies by associating the child devices as members of a graph specific only to their parent node?

## Lists

1. How on earth do mpr_lists work?

2. Do mpr_lists need to be initialized in some way? OR do you start adding items immediately with mpr_list_add_item() ??

3. Will a list from data work to make a mpr_list from a reference in the struct, no query necessary?

4. Can you insert an existing item into a list? Or must you init a new one and copy details?

5. Proper way to use a list?

## Enums / MPR Constants

1. How are the values determined for the ENUMs such as DEV type?

## Makefile

1. Where in makefiles (or elsewhere) do I add options for more C lib tests?

## General libmapper

1. What are the PROPS for? Metadata?

2. Why must you add them alphabetically?

## Questions about honors project

1. Scope?
2. Publication of named instances / hierarchial structures.
3. Anything else for me?

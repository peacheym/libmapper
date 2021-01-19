# Questions to ask while implementing libmapper functionalities.

1) Difference between dev and dev->loc ???

1a) Significance of local device, why is it important?

2) To implement hierarchial structures, can we add a mpr_list of mpr_devices as an optional attribute of the mpr_dev struct?

2a) Would it be better to implement a new "child node" type of MPR object? Issue I see here is that it would allow only a depth of 1 child from a parent, no grandchildren or further.
2b) Would it be better to fake the hierarchies by associating the child devices as members of a graph specific only to their parent node?

3) How on earth to mpr_lists work? 

3a) Do mpr_lists need to be initialized in some way?  OR do you start adding items immediately with mpr_list_add_item() ??

3b) Will a list from data work to make a mpr_list from a reference in the struct, no query necessary?

3c) Can you insert an existing item into a list?  Ie a dev?

4) Where in makefiles (or elsewhere) do I add options for more C lib tests?

5) How are the values determined for the ENUMs such as DEV type?


6) Should there be some way to copy mpr_objs for passing both a child and a parent, add to list and then copy attributes from passed child to new list item???
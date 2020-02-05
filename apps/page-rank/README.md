# Page Rank

This accelerator is designed to perform a double-phase update scheme with
pre-partitioned graph data.

## Load Balancing

Before doing partitioning, the vertex ids (vids) are transformed for better
  load-balancing.
The preprocessor will first find the min and max vids.
The min vid will be used as the base vid.
The partition size is a give parameter.
The number of partitions is calculated based on the number of vertices and the
  partition size.
Each vid is then mapped to `max(1, min_vid) + (vid - min_vid) % num_partitions *
  partition_size + (vid - min_vid) / num_partitions`.
`0` is excluded from possible vids to represent null edges.

## PE Assignment

Each PE is assigned a set of partitions based on the partition id modulo the
  number of PEs.
In the scatter phase,
  each PE only processes edges whose destination vertices are in their assigned
  partitions and thus only generate updates to these partitions.
In the gather phase,
  each PE processes the updates generated from the previous stage.

## Vectorization

Within each PE,
  the edges and updates are vectorized to best-utilize the memory bandwidth.
Let the length of each vector be `N`.
The edges are assembled such that edge `i` in each vector has relative source
  vertex id `i` modulo `N`,
  and all destination vertices are different modulo `N`.
The PE will assemble the updates such that update `i` in each vector has
  relative destination vertex id `i` modulo `N`.

## Edge List Processing

Let the number of partitions be `P`.
We first partition the edges into `P*P*N*N` bins.
Each bin is indexed by the source and destination partition id and the source
  and destination bank id.
This step can be done in `O(m)` time where `m` is the number of the edges.
It is also possible to efficiently implement on FPGA.

Next, we assemble the edge vectors from each the bins.
We iterate over the subshards one by one.
In each subshard,
  we select edges from diagonals of the banks until there is no edge left in
  that subshard in empty.
Empty edges are added if a bin of edges runs out.
When assembling each vector, we only select from the one side of the bins,
  and the bins only need to be visited sequentially.
Moreover, when selecting edges,
  we look back for certain number of elements to guarantee relaxed dependence
  constraint in the PE.
This step can also be done in `O(m)` time and can be offloaded to FPGA.

## Kernel Code Design

+ Initialization
  + `Control` tells `UpdateHandler` the offsets of each `Update` partition
+ `kScatter` phase
  + `Control` sends out `TaskReq` with `Vertex` and `Edge` partitions to
    `ProcElem` and collects `TaskResp` from them
  + `ProcElem` sends `Update`
  + `UpdateHandler` collects `Update`
+ `kGather` phase
  + `Control` sets all `UpdateHandler` to `kGather`
  + `Control` sends out `TaskReq` with `Vertex` partitions to `ProcElem` and
    collects `TaskResp` from them
  + `ProcElem` requests `Update` from `UpdateHandler`
  + `UpdateHandler` sends `Update`

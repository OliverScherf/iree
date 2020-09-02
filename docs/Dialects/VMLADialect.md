---
layout: default
permalink: Dialects/VMLADialect
title: "'vmla' Dialect"
parent: Dialect Definitions
---

<!-- Autogenerated by mlir-tblgen; don't manually edit -->
# 'vmla' Dialect
{: .no_toc }


A dialect representing operations against the IREE VM-based backend.


This is a reference dialect representing a simple IREE VM-based linear
algebra module that is used as a library at runtime. The ops in this dialect
map (roughly) 1:1 with the exported functions in the runtime module.

See `vmla.imports.mlir` for the full list of exported functions.

1. TOC
{:toc}

## Type definition

### buffer

A lightweight unshaped byte buffer.

### interface

Binding and parameter interface (derived from `hal.interface`).

## Operation definition

### `vmla.conv` (IREE::VMLA::ConvOp)



Syntax:

```
operation ::= `vmla.conv` $input`(`$input_shape `:` type($input_shape)`)` `:` $input_type`,`
              $filter`(`$filter_shape `:` type($filter_shape)`)` `:` $filter_type`,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` `:` $dst_type attr-dict
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`window_strides` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`padding` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`lhs_dilation` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`rhs_dilation` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`feature_group_count` | ::mlir::IntegerAttr | 32-bit signless integer attribute
`batch_group_count` | ::mlir::IntegerAttr | 32-bit signless integer attribute
`input_type` | ::mlir::TypeAttr | any type attribute
`filter_type` | ::mlir::TypeAttr | any type attribute
`dst_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`input` | buffer
`input_shape` | Ranked shape type
`filter` | buffer
`filter_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.abs` (IREE::VMLA::AbsOp)



Syntax:

```
operation ::= `vmla.abs` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.add` (IREE::VMLA::AddOp)



Syntax:

```
operation ::= `vmla.add` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.and` (IREE::VMLA::AndOp)



Syntax:

```
operation ::= `vmla.and` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.atan2` (IREE::VMLA::Atan2Op)



Syntax:

```
operation ::= `vmla.atan2` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.batch.matmul` (IREE::VMLA::BatchMatMulOp)



Syntax:

```
operation ::= `vmla.batch.matmul` $lhs`(`$lhs_shape `:` type($lhs_shape)`)` `:` $lhs_type`,`
              $rhs`(`$rhs_shape `:` type($rhs_shape)`)` `:` $rhs_type`,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` `:` $dst_type attr-dict
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`lhs_type` | ::mlir::TypeAttr | any type attribute
`rhs_type` | ::mlir::TypeAttr | any type attribute
`dst_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`lhs_shape` | Ranked shape type
`rhs` | buffer
`rhs_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.batch.matmul.pseudo` (IREE::VMLA::BatchMatMulPseudoOp)

Tensor-level pseudo-op of VMLA::BatchMatMulOp.

Syntax:

```
operation ::= `vmla.batch.matmul.pseudo` $lhs`,` $rhs attr-dict `:`
              `(`type($lhs)`,` type($rhs)`)` `->` type($dst)
```


This is a tensor-level version of VMLA::BatchMatMulOp, to facilitate
the lowering process.

All operands are rank-3 with the following dimension structure:
- lhs = [B, FLHS, C]
- rhs = [B, FRHS, C]
- dst = [B, FRHS, FLHS]
Where:
- B = batch dimension
- C = contracting dimension
- FLHS and FRHS are the free dimensions of each operand

To put this in terms closer to the mathematics of matrix multiplication,
if we ignore the leading B dimension and focus on what is mathematically an
MxKxN matmul, then this corresponds to:
- lhs = [M, K] = [LHSROWS, K]
- rhs = [N, K] = [RHSCOLS, K]
- dst = [N, M] = [RHSCOLS, LHSROWS]
Note that dst is transposed from what one would expect.
This is due to an implementation detail of this op in the runtime.
This op is backed by an invocation of the Ruy matrix multiplication library,
which prefers its matrices in this layout (in matrix terminology:
lhs = row-major, rhs = column-major, dst = column-major).
We insert the relevant transposes as needed in the compiler.

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | tensor of any type values
`rhs` | tensor of any type values

#### Results:

| Result | Description |
| :----: | ----------- |
`dst` | tensor of any type values

### `vmla.broadcast` (IREE::VMLA::BroadcastOp)



Syntax:

```
operation ::= `vmla.broadcast` $src`(`$src_shape `:` type($src_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.buffer.alloc` (IREE::VMLA::BufferAllocOp)



Syntax:

```
operation ::= `vmla.buffer.alloc` `byte_length` `=` $byte_length attr-dict `:` type($result)
```



#### Operands:

| Operand | Description |
| :-----: | ----------- |
`byte_length` | index

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | buffer

### `vmla.buffer.byte_length` (IREE::VMLA::BufferByteLengthOp)



Syntax:

```
operation ::= `vmla.buffer.byte_length` $value attr-dict `:` type($result)
```



#### Operands:

| Operand | Description |
| :-----: | ----------- |
`value` | buffer

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | index

### `vmla.buffer.clone` (IREE::VMLA::BufferCloneOp)



Syntax:

```
operation ::= `vmla.buffer.clone` $src attr-dict `:` type($result)
```



#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | buffer

### `vmla.buffer.const` (IREE::VMLA::BufferConstOp)



Syntax:

```
operation ::= `vmla.buffer.const` $value attr-dict `:` type($value) `->` type($result)
```



#### Operands:

| Operand | Description |
| :-----: | ----------- |
`value` | byte_buffer or mutable_byte_buffer

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | buffer

### `vmla.buffer.copy` (IREE::VMLA::BufferCopyOp)



Syntax:

```
operation ::= `vmla.buffer.copy` $src`[`$src_byte_offset`]``,`
              `out` $dst`[`$dst_byte_offset`]``,` `byte_length` `=` $byte_length
              attr-dict
```



#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_byte_offset` | index
`dst` | buffer
`dst_byte_offset` | index
`byte_length` | index

### `vmla.buffer.fill` (IREE::VMLA::BufferFillOp)



Syntax:

```
operation ::= `vmla.buffer.fill` $value`,` `out` $dst attr-dict
```



#### Operands:

| Operand | Description |
| :-----: | ----------- |
`value` | buffer
`dst` | buffer

### `vmla.buffer.load.i32` (IREE::VMLA::BufferLoadI32Op)



Syntax:

```
operation ::= `vmla.buffer.load.i32` $src`[`$byte_offset`]` attr-dict `:` type($result)
```



#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`byte_offset` | index

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | 32-bit signless integer

### `vmla.buffer.view` (IREE::VMLA::BufferViewOp)



Syntax:

```
operation ::= `vmla.buffer.view` $src`[`$byte_offset`]``,` `byte_length` `=` $byte_length
              attr-dict `:` type($result)
```



#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`byte_offset` | index
`byte_length` | index

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | buffer

### `vmla.ceil` (IREE::VMLA::CeilOp)



Syntax:

```
operation ::= `vmla.ceil` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.clamp` (IREE::VMLA::ClampOp)



Syntax:

```
operation ::= `vmla.clamp` $a`,` $b`,` $c`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`a` | buffer
`b` | buffer
`c` | buffer
`dst` | buffer

### `vmla.cmp` (IREE::VMLA::CmpOp)



Syntax:

```
operation ::= `vmla.cmp` $predicate`,` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`predicate` | ::mlir::IntegerAttr | IREE VMLA comparison op predicate
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.constant` (IREE::VMLA::ConstantOp)

constant buffer declaration

Syntax:

```
operation ::= `vmla.constant` attr-dict $value `->` type($result)
```


A pseudo-op used to represent a buffer with constant contents. This is later
expanded into VM ops and the vmla.buffer.const op.

#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`value` | ::mlir::ElementsAttr | constant vector/tensor attribute

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | buffer

### `vmla.convert` (IREE::VMLA::ConvertOp)



Syntax:

```
operation ::= `vmla.convert` $src`,` `out` $dst attr-dict `:` $src_type `->` $dst_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`src_type` | ::mlir::TypeAttr | any type attribute
`dst_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.copy` (IREE::VMLA::CopyOp)



Syntax:

```
operation ::= `vmla.copy` $src`(`$src_shape `:` type($src_shape)`)``,`
              (`src_indices` `=` `[` $src_indices^ `]``,`)?
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)`
              (`,` `dst_indices` `=` `[` $dst_indices^ `]`)?
              (`,` `lengths` `=` `[` $lengths^ `]`)? attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`src_indices` | index
`dst` | buffer
`dst_shape` | Ranked shape type
`dst_indices` | index
`lengths` | index

### `vmla.cos` (IREE::VMLA::CosOp)



Syntax:

```
operation ::= `vmla.cos` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.div` (IREE::VMLA::DivOp)



Syntax:

```
operation ::= `vmla.div` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.exp` (IREE::VMLA::ExpOp)



Syntax:

```
operation ::= `vmla.exp` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.finite` (IREE::VMLA::FiniteOp)



Syntax:

```
operation ::= `vmla.finite` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.floor` (IREE::VMLA::FloorOp)



Syntax:

```
operation ::= `vmla.floor` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.gather` (IREE::VMLA::GatherOp)



Syntax:

```
operation ::= `vmla.gather` $src`(`$src_shape `:` type($src_shape)`)``,`
              $indices`(`$indices_shape `:` type($indices_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`dim` | ::mlir::IntegerAttr | 64-bit signless integer attribute
`batch_dims` | ::mlir::IntegerAttr | 64-bit signless integer attribute
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`indices` | buffer
`indices_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.interface.binding` (IREE::VMLA::InterfaceBindingOp)



Syntax:

```
operation ::= `vmla.interface.binding` $interface attr-dict `:` type($result)
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`set` | ::mlir::IntegerAttr | 32-bit signless integer attribute
`binding` | ::mlir::IntegerAttr | 32-bit signless integer attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`interface` | interface

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | buffer

### `vmla.interface.const` (IREE::VMLA::InterfaceConstOp)



Syntax:

```
operation ::= `vmla.interface.const` $interface attr-dict `:` type($result)
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`offset` | ::mlir::IntegerAttr | size_t

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`interface` | interface

#### Results:

| Result | Description |
| :----: | ----------- |
`result` | 32-bit signless integer or index

### `vmla.iota` (IREE::VMLA::IotaOp)



Syntax:

```
operation ::= `vmla.iota` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`dst` | buffer

### `vmla.log` (IREE::VMLA::LogOp)



Syntax:

```
operation ::= `vmla.log` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.max` (IREE::VMLA::MaxOp)



Syntax:

```
operation ::= `vmla.max` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.min` (IREE::VMLA::MinOp)



Syntax:

```
operation ::= `vmla.min` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.mul` (IREE::VMLA::MulOp)



Syntax:

```
operation ::= `vmla.mul` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.neg` (IREE::VMLA::NegOp)



Syntax:

```
operation ::= `vmla.neg` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.not` (IREE::VMLA::NotOp)



Syntax:

```
operation ::= `vmla.not` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.or` (IREE::VMLA::OrOp)



Syntax:

```
operation ::= `vmla.or` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.pad` (IREE::VMLA::PadOp)



Syntax:

```
operation ::= `vmla.pad` $src`(`$src_shape `:` type($src_shape)`)``,`
              $value`(`$value_shape `:` type($value_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`edge_padding_low` | ::mlir::ElementsAttr | constant vector/tensor attribute
`edge_padding_high` | ::mlir::ElementsAttr | constant vector/tensor attribute
`interior_padding` | ::mlir::ElementsAttr | constant vector/tensor attribute
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`value` | buffer
`value_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.pooling.max` (IREE::VMLA::PoolingMaxOp)



Syntax:

```
operation ::= `vmla.pooling.max` $src`(`$src_shape `:` type($src_shape)`)``,`
              $init`(`$init_shape `:` type($init_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`window_dimensions` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`window_strides` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`padding` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`init` | buffer
`init_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.pooling.min` (IREE::VMLA::PoolingMinOp)



Syntax:

```
operation ::= `vmla.pooling.min` $src`(`$src_shape `:` type($src_shape)`)``,`
              $init`(`$init_shape `:` type($init_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`window_dimensions` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`window_strides` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`padding` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`init` | buffer
`init_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.pooling.sum` (IREE::VMLA::PoolingSumOp)



Syntax:

```
operation ::= `vmla.pooling.sum` $src`(`$src_shape `:` type($src_shape)`)``,`
              $init`(`$init_shape `:` type($init_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`window_dimensions` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`window_strides` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute
`padding` | ::mlir::DenseIntElementsAttr | 32-bit signless integer elements attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`init` | buffer
`init_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.pow` (IREE::VMLA::PowOp)



Syntax:

```
operation ::= `vmla.pow` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.reduce.max` (IREE::VMLA::ReduceMaxOp)



Syntax:

```
operation ::= `vmla.reduce.max` $src`(`$src_shape `:` type($src_shape)`)``,`
              $init`(`$init_shape `:` type($init_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`dimension` | ::mlir::IntegerAttr | 32-bit signless integer attribute
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`init` | buffer
`init_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.reduce.min` (IREE::VMLA::ReduceMinOp)



Syntax:

```
operation ::= `vmla.reduce.min` $src`(`$src_shape `:` type($src_shape)`)``,`
              $init`(`$init_shape `:` type($init_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`dimension` | ::mlir::IntegerAttr | 32-bit signless integer attribute
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`init` | buffer
`init_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.reduce.sum` (IREE::VMLA::ReduceSumOp)



Syntax:

```
operation ::= `vmla.reduce.sum` $src`(`$src_shape `:` type($src_shape)`)``,`
              $init`(`$init_shape `:` type($init_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`dimension` | ::mlir::IntegerAttr | 32-bit signless integer attribute
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`init` | buffer
`init_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.rem` (IREE::VMLA::RemOp)



Syntax:

```
operation ::= `vmla.rem` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.reverse` (IREE::VMLA::ReverseOp)



Syntax:

```
operation ::= `vmla.reverse` $src`(`$src_shape `:` type($src_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`dimensions` | ::mlir::ElementsAttr | constant vector/tensor attribute
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.rsqrt` (IREE::VMLA::RsqrtOp)



Syntax:

```
operation ::= `vmla.rsqrt` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.scatter` (IREE::VMLA::ScatterOp)



Syntax:

```
operation ::= `vmla.scatter` $src`(`$src_shape `:` type($src_shape)`)``,`
              $indices`(`$indices_shape `:` type($indices_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`indices` | buffer
`indices_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.select` (IREE::VMLA::SelectOp)



Syntax:

```
operation ::= `vmla.select` $cond`,` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`cond` | buffer
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.shl` (IREE::VMLA::ShlOp)



Syntax:

```
operation ::= `vmla.shl` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.shr` (IREE::VMLA::ShrOp)



Syntax:

```
operation ::= `vmla.shr` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.sin` (IREE::VMLA::SinOp)



Syntax:

```
operation ::= `vmla.sin` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.sqrt` (IREE::VMLA::SqrtOp)



Syntax:

```
operation ::= `vmla.sqrt` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.sub` (IREE::VMLA::SubOp)



Syntax:

```
operation ::= `vmla.sub` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer

### `vmla.tanh` (IREE::VMLA::TanhOp)



Syntax:

```
operation ::= `vmla.tanh` $src`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`dst` | buffer

### `vmla.tile` (IREE::VMLA::TileOp)



Syntax:

```
operation ::= `vmla.tile` $src`(`$src_shape `:` type($src_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.transpose` (IREE::VMLA::TransposeOp)



Syntax:

```
operation ::= `vmla.transpose` $src`(`$src_shape `:` type($src_shape)`)``,`
              `out` $dst`(`$dst_shape `:` type($dst_shape)`)` attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`permutation` | ::mlir::ElementsAttr | constant vector/tensor attribute
`element_type` | ::mlir::TypeAttr | any type attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`src` | buffer
`src_shape` | Ranked shape type
`dst` | buffer
`dst_shape` | Ranked shape type

### `vmla.xor` (IREE::VMLA::XorOp)



Syntax:

```
operation ::= `vmla.xor` $lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type
```



#### Attributes:

| Attribute | MLIR Type | Description |
| :-------: | :-------: | ----------- |
`element_type` | ::mlir::TypeAttr | any type attribute
`forceUnsigned` | ::mlir::UnitAttr | unit attribute

#### Operands:

| Operand | Description |
| :-----: | ----------- |
`lhs` | buffer
`rhs` | buffer
`dst` | buffer
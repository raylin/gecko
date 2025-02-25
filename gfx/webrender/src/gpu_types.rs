/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{LayerVector2D, LayerRect, LayerToWorldTransform, WorldToLayerTransform};
use gpu_cache::GpuCacheAddress;
use render_task::RenderTaskAddress;

// Contains type that must exactly match the same structures declared in GLSL.

#[repr(i32)]
#[derive(Debug, Copy, Clone)]
pub enum BlurDirection {
    Horizontal = 0,
    Vertical,
}

#[derive(Debug)]
#[repr(C)]
pub struct BlurInstance {
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
    pub blur_direction: BlurDirection,
    pub region: LayerRect,
}

/// A clipping primitive drawn into the clipping mask.
/// Could be an image or a rectangle, which defines the
/// way `address` is treated.
#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct ClipMaskInstance {
    pub render_task_address: RenderTaskAddress,
    pub scroll_node_data_index: ClipScrollNodeIndex,
    pub segment: i32,
    pub clip_data_address: GpuCacheAddress,
    pub resource_address: GpuCacheAddress,
}

// 32 bytes per instance should be enough for anyone!
#[derive(Debug, Clone)]
pub struct PrimitiveInstance {
    data: [i32; 8],
}

pub struct SimplePrimitiveInstance {
    pub specific_prim_address: GpuCacheAddress,
    pub task_address: RenderTaskAddress,
    pub clip_task_address: RenderTaskAddress,
    pub clip_id: ClipScrollNodeIndex,
    pub scroll_id: ClipScrollNodeIndex,
    pub z_sort_index: i32,
}

impl SimplePrimitiveInstance {
    pub fn new(
        specific_prim_address: GpuCacheAddress,
        task_address: RenderTaskAddress,
        clip_task_address: RenderTaskAddress,
        clip_id: ClipScrollNodeIndex,
        scroll_id: ClipScrollNodeIndex,
        z_sort_index: i32,
    ) -> SimplePrimitiveInstance {
        SimplePrimitiveInstance {
            specific_prim_address,
            task_address,
            clip_task_address,
            clip_id,
            scroll_id,
            z_sort_index,
        }
    }

    pub fn build(&self, data0: i32, data1: i32, data2: i32) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                self.specific_prim_address.as_int(),
                self.task_address.0 as i32,
                self.clip_task_address.0 as i32,
                ((self.clip_id.0 as i32) << 16) | self.scroll_id.0 as i32,
                self.z_sort_index,
                data0,
                data1,
                data2,
            ],
        }
    }
}

pub struct CompositePrimitiveInstance {
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
    pub backdrop_task_address: RenderTaskAddress,
    pub data0: i32,
    pub data1: i32,
    pub z: i32,
    pub data2: i32,
    pub data3: i32,
}

impl CompositePrimitiveInstance {
    pub fn new(
        task_address: RenderTaskAddress,
        src_task_address: RenderTaskAddress,
        backdrop_task_address: RenderTaskAddress,
        data0: i32,
        data1: i32,
        z: i32,
        data2: i32,
        data3: i32,
    ) -> CompositePrimitiveInstance {
        CompositePrimitiveInstance {
            task_address,
            src_task_address,
            backdrop_task_address,
            data0,
            data1,
            z,
            data2,
            data3,
        }
    }
}

impl From<CompositePrimitiveInstance> for PrimitiveInstance {
    fn from(instance: CompositePrimitiveInstance) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                instance.task_address.0 as i32,
                instance.src_task_address.0 as i32,
                instance.backdrop_task_address.0 as i32,
                instance.z,
                instance.data0,
                instance.data1,
                instance.data2,
                instance.data3,
            ],
        }
    }
}

// Whether this brush is being drawn on a Picture
// task (new) or an alpha batch task (legacy).
// Can be removed once everything uses pictures.
pub const BRUSH_FLAG_USES_PICTURE: i32 = (1 << 0);

// TODO(gw): While we are comverting things over, we
//           need to have the instance be the same
//           size as an old PrimitiveInstance. In the
//           future, we can compress this vertex
//           format a lot - e.g. z, render task
//           addresses etc can reasonably become
//           a u16 type.
#[repr(C)]
pub struct BrushInstance {
    pub picture_address: RenderTaskAddress,
    pub prim_address: GpuCacheAddress,
    pub clip_id: ClipScrollNodeIndex,
    pub scroll_id: ClipScrollNodeIndex,
    pub clip_task_address: RenderTaskAddress,
    pub z: i32,
    pub flags: i32,
    pub user_data0: i32,
    pub user_data1: i32,
}

impl From<BrushInstance> for PrimitiveInstance {
    fn from(instance: BrushInstance) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                instance.picture_address.0 as i32,
                instance.prim_address.as_int(),
                ((instance.clip_id.0 as i32) << 16) | instance.scroll_id.0 as i32,
                instance.clip_task_address.0 as i32,
                instance.z,
                instance.flags,
                instance.user_data0,
                instance.user_data1,
            ]
        }
    }
}

// Defines how a brush image is stretched onto the primitive.
// In the future, we may draw with segments for each portion
// of the primitive, in which case this will be redundant.
#[repr(C)]
pub enum BrushImageKind {
    Simple = 0,     // A normal rect
    NinePatch = 1,  // A nine-patch image (stretch inside segments)
    Mirror = 2,     // A top left corner only (mirror across x/y axes)
}

#[derive(Copy, Debug, Clone)]
#[repr(C)]
pub struct ClipScrollNodeIndex(pub u32);

#[derive(Debug)]
#[repr(C)]
pub struct ClipScrollNodeData {
    pub transform: LayerToWorldTransform,
    pub inv_transform: WorldToLayerTransform,
    pub local_clip_rect: LayerRect,
    pub reference_frame_relative_scroll_offset: LayerVector2D,
    pub scroll_offset: LayerVector2D,
}

impl ClipScrollNodeData {
    pub fn invalid() -> ClipScrollNodeData {
        ClipScrollNodeData {
            transform: LayerToWorldTransform::identity(),
            inv_transform: WorldToLayerTransform::identity(),
            local_clip_rect: LayerRect::zero(),
            reference_frame_relative_scroll_offset: LayerVector2D::zero(),
            scroll_offset: LayerVector2D::zero(),
        }
    }
}

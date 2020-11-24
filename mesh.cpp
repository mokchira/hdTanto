//
// Copyright 2020 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "mesh.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

HdTantoMesh::HdTantoMesh(SdfPath const& id, SdfPath const& instancerId)
    : HdMesh(id, instancerId)
{
}

HdDirtyBits
HdTantoMesh::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::Clean
        | HdChangeTracker::DirtyTransform
        | HdChangeTracker::InitRepr
        | HdChangeTracker::DirtyPoints
        | HdChangeTracker::DirtyTopology
        | HdChangeTracker::DirtyTransform
        | HdChangeTracker::DirtyVisibility
        | HdChangeTracker::DirtyCullStyle;
}

HdDirtyBits
HdTantoMesh::_PropagateDirtyBits(HdDirtyBits bits) const
{
    return bits;
}

void 
HdTantoMesh::_InitRepr(TfToken const &reprToken, HdDirtyBits *dirtyBits)
{
    TF_UNUSED(dirtyBits);

    std::cout << "_InitRepr. ReprToken: " << reprToken << '\n';

    // Create an empty repr.
    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                            _ReprComparator(reprToken));
    if (it == _reprs.end()) {
        _reprs.emplace_back(reprToken, HdReprSharedPtr());
    }
}

void
HdTantoMesh::Sync(HdSceneDelegate *sceneDelegate,
                   HdRenderParam   *renderParam,
                   HdDirtyBits     *dirtyBits,
                   TfToken const   &reprToken)
{
    std::cout << "* (multithreaded) Sync Tanto Mesh id=" << GetId() << std::endl;
    //
    // XXX: A mesh repr can have multiple repr decs; this is done, for example, 
    // when the drawstyle specifies different rasterizing modes between front
    // faces and back faces.
    // With raytracing, this concept makes less sense, but
    // combining semantics of two HdMeshReprDesc is tricky in the general case.
    // For now, HdEmbreeMesh only respects the first desc; this should be fixed.
    _MeshReprConfig::DescArray descs = _GetReprDesc(reprToken);
    const HdMeshReprDesc &desc = descs[0];

    // Pull top-level embree state out of the render param.
    // Create embree geometry objects.
    _PopulateTantoMesh(sceneDelegate, dirtyBits, desc);
}

void HdTantoMesh::_PopulateTantoMesh(HdSceneDelegate *sceneDelegate,
                         HdDirtyBits *dirtyBits,
                         HdMeshReprDesc const &desc)
{
    SdfPath const& id = GetId();

    ////////////////////////////////////////////////////////////////////////
    // 1. Pull scene data.

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)) 
    {
        VtValue value = sceneDelegate->Get(id, HdTokens->points);
        _points = value.Get<VtVec3fArray>();
        std::cout << "Points dirty!!" << '\n';
    }

    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) 
    {
        // When pulling a new topology, we don't want to overwrite the
        // refine level or subdiv tags, which are provided separately by the
        // scene delegate, so we save and restore them.
        PxOsdSubdivTags subdivTags = _topology.GetSubdivTags();
        int refineLevel = _topology.GetRefineLevel();
        _topology = HdMeshTopology(GetMeshTopology(sceneDelegate), refineLevel);
        _topology.SetSubdivTags(subdivTags);
        std::cout << "Topology dirty!!" << '\n';
    }

    if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) 
    {
        _transform = GfMatrix4f(sceneDelegate->GetTransform(id));
        std::cout << "Transform dirty!!" << '\n';
    }
}


PXR_NAMESPACE_CLOSE_SCOPE
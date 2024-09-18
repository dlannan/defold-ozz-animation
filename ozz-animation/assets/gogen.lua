-- Helper to generate gos, with meshes and buffers as needed by the anim

local tinsert = table.insert

local gogen = {
    ctr = 1,
    gos = {},
}

gogen.makeGameObject = function( factory_url, animid, meshid, iverts )
	local newgo = {
		animid = animid,
		meshid = meshid,
		url = factory.create(factory_url, nil, nil),
	}

	-- Create buffers and streams
	newgo.mesh_url = msg.url("main", newgo.url, "mesh")
	local res = resource.get_buffer(go.get(newgo.mesh_url, "vertices"))	

    -- create a cloned buffer resource from another resource buffer
	newgo.meshbuf = ozzanim.createbuffers(iverts, newgo.animid, newgo.meshid)

	-- set the buffer with the vertices on the mesh
    local resourcename = "/mesh_buffer_"..string.format("%d", gogen.ctr)..".bufferc"
	newgo.res = resource.create_buffer(resourcename, { buffer = res })	
	gogen.ctr = gogen.ctr + 1
		
	resource.set_buffer(newgo.res, newgo.meshbuf)
	go.set(newgo.mesh_url, "vertices", newgo.res )
	--go.get(mesh_url, "material")
	return newgo
end

gogen.makeParentGo = function( factory_url, animid, data )

	local parent = { animid = animid, meshid = nil, url = factory.create(factory_url, nil, nil), }
    parent.children = {}
	for i,v in ipairs(data) do
		local child = gogen.makeGameObject(factory_url, animid, i-1, v.triangle_index_count)
		go.set_parent(child.url, parent.url)
        tinsert(parent.children, child)
	end
	return parent
end

gogen.drawSkinned = function( parent, anim )

    for i, v in ipairs(parent.children) do       
        ozzanim.drawskinnedmesh(anim)
        resource.set_buffer(v.res, v.meshbuf)
    end
end

return gogen
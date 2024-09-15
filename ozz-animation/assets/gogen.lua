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
	pprint(newgo.url)
	local mesh_url = msg.url("main", newgo.url, "mesh")
	local res = resource.get_buffer(go.get(mesh_url, "vertices"))	

    -- create a cloned buffer resource from another resource buffer
	local newres = resource.create_buffer("/mesh_buffer_"..string.format("%d", gogen.ctr)..".bufferc", { buffer = res })	
	gogen.ctr = gogen.ctr + 1

	local meshdata = {}
	-- positions are required (should assert or something)
	tinsert(meshdata, { name = hash("position"), type=buffer.VALUE_TYPE_FLOAT32, count = 3 } )
	-- if(normals) then tinsert(meshdata, { name = hash("normal"), type=buffer.VALUE_TYPE_FLOAT32, count = 3 } ) end
	-- if(uvs) then tinsert(meshdata, { name = hash("texcoord0"), type=buffer.VALUE_TYPE_FLOAT32, count = 2 } ) end
		
	local meshbuf = buffer.create(iverts, meshdata)	
	ozzanim.setbufferfrommesh( meshbuf, "position", newgo.animid, newgo.meshid )
			
	-- if(normals) then geomextension.setbufferbytesfromtable( meshbuf, "normal", indices, normals ) end
	-- if(uvs) then geomextension.setbufferbytesfromtable( meshbuf, "texcoord0", indices, uvs ) end 

	-- set the buffer with the vertices on the mesh
	resource.set_buffer(newres, meshbuf)
	go.set(mesh_url, "vertices", newres )
	go.get(mesh_url, "material")
	return newgo
end

gogen.makeParentGo = function( factory_url, animid, data )

	local parent = { animid = animid, meshid = nil, url = factory.create(factory_url, nil, nil), }
    parent.children = {}
	for i,v in ipairs(data) do
		local child = gogen.makeGameObject(factory_url, animid, i, v.triangle_index_count)
        tinsert(parent.children, child)
	end
	return parent
end

return gogen
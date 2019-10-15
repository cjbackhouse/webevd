// TODO learn how imports/modules work
//import { IcosahedronBufferGeometry } from './three.js-master/src/geometries/IcosahedronGeometry.js'

var scene = new THREE.Scene();

//var camera = new THREE.PerspectiveCamera( 75, window.innerWidth / window.innerHeight, 0.1, 1e6 );

var camera = new THREE.OrthographicCamera( -512, +512,
                                           -512*window.innerHeight/window.innerWidth, +512*window.innerHeight/window.innerWidth,
                                           0.1, 1000 );

var renderer = new THREE.WebGLRenderer();
// I have no idea why these 4 pixels are necessary
renderer.setSize( window.innerWidth, window.innerHeight-4);

renderer.setClearColor('black');//'white');//0xa0a0a0);

renderer.alpha = true;
renderer.antialias = true;

document.body.appendChild( renderer.domElement );

// Only looks good on initial page load :(
//renderer.domElement.style.width = '100%';
//renderer.domElement.style.height = '100%';

//var texture = new THREE.TextureLoader().load( "texture.jpg" );
//texture.wrapS = THREE.RepeatWrapping;
//texture.wrapT = THREE.RepeatWrapping;
//texture.repeat.set( 4, 4 );

var size = 256*256;//width * height;
var data = new Uint8Array( 3 * size );

for ( var i = 0; i < size; i ++ ) {

    var stride = i * 3;

    c = (i%256 + i/256)/2;
    data[ stride ] = c;
    data[ stride + 1 ] = c;
    data[ stride + 2 ] = c;

}

// used the buffer to create a DataTexture

var texture = new THREE.DataTexture( data, 256, 256, THREE.RGBFormat )

var geometry = new THREE.BoxBufferGeometry( .4, .4, .4);//1, 1, 1 );
var material = new THREE.MeshBasicMaterial( { color: 0x0000ff, map: texture } );

//var geometry = new IcosahedronBufferGeometry({radius: .4});
// var cube = new THREE.Mesh( geometry, material );
// scene.add( cube );


//var mat2 = new THREE.MeshBasicMaterial( { color: 0xff0000 } );
// mat2.map = texture;

// var cube2 = new THREE.Mesh( geometry, mat2 );
// cube2.position.x += 2;

// scene.add( cube2 );

var group = new THREE.Group();

var mx = 0, my = 0, mz = 0;
for(var i = 0; i < coords.length; ++i){
    var x, y, z;
    [x, y, z] = coords[i];
    mx += x; my += y; mz += z;
    var c = new THREE.Mesh( geometry, material );
    c.position.x = z;//x;
    c.position.y = x;//y;
    c.position.z = .5;//z;
//    console.log(i);
//    console.log(coords[i]);
//    console.log(coords[i][0] + ", " + coords[i][1] + "," + coords[i][2]);
    group.add(c);
}

mx /= coords.length;
my /= coords.length;
mz /= coords.length;

// center
//group.position.set(-mx, -my, -mz);

group.layers.set(2); // we made the Z view projection for now

scene.add(group);


//var geometry = new THREE.PlaneGeometry( 512, 512);
//var material = new THREE.MeshBasicMaterial( {color: 0xff0000, side: THREE.DoubleSide, transparent: true} );

//var matdig_neg = new THREE.MeshBasicMaterial( {color: 0x0000ff, side: THREE.DoubleSide, transparent: true} );

//var matlin = new THREE.LineBasicMaterial( { color: 'gray'});//0x000000 } );
//var outlinegeom = new THREE.Geometry();
//outlinegeom.vertices.push(new THREE.Vector3( -256, -256, 0) );
//outlinegeom.vertices.push(new THREE.Vector3( +256, -256, 0) );
//outlinegeom.vertices.push(new THREE.Vector3( +256, +256, 0) );
//outlinegeom.vertices.push(new THREE.Vector3( -256, +256, 0) );
//outlinegeom.vertices.push(new THREE.Vector3( -256, -256, 0) );
//var line = new THREE.LineLoop( outlinegeom, matlin );
//scene.add( line );


/*
var size = 512*512*8*8;
var data = new Uint8Array( 4 * size );
//for(var i = 0; i < size*4; ++i) data[i] = 64;

// for(var i = 0; i < coords.length; ++i){
//     var x, y, z;
//     [x, y, z] = coords[i];

// //    data[(Math.floor(x-mx+128)+512*Math.floor(z-mz+128))*3] = 255;
//     data[(Math.floor(x-mx+128)+512*Math.floor(y-my+128))*4] = 255;
//     data[(Math.floor(x-mx+128)+512*Math.floor(y-my+128))*4+3] = 255;
// }

console.log(waves.length);
console.log(waves[0].length);

for(var x = 0; x < 512*8; ++x){
    for(var y = 0; y < 512*8; ++y){
        adc = waves[x][y];
        var i = x+512*8*y;
        if(adc > 0) data[i*4] = 255; else data[i*4+2] = 255;//Math.min(adc, 255); else data[i*4+2] = Math.min(-adc, 255);

//        data[i*4+3] = 255;
//        if(adc != 0){
            data[i*4+3] = Math.min(Math.abs(adc), 255); // transparent when zero
//            console.log(adc);
//            console.log(Math.abs(adc));
//            console.log("");
//        }
    }
}
*/

// used the buffer to create a DataTexture


// var texdig = new THREE.TextureLoader().load("digits_C:0 T:9 P:2_pos.png");//"digits_pos.png");
// material.alphaMap = texdig;
// texdig.magFilter = THREE.NearestFilter;
// texdig.minFilter = THREE.LinearFilter;
// texdig.generateMipmaps = false;

// var texdig_neg = new THREE.TextureLoader().load("digits_C:0 T:9 P:2_neg.png");//"digits_neg.png");
// matdig_neg.alphaMap = texdig_neg;
// texdig_neg.magFilter = THREE.NearestFilter;
// texdig_neg.minFilter = THREE.LinearFilter;
// texdig_neg.generateMipmaps = false;

// //var texture = new THREE.DataTexture( data, 512*8, 512*8, THREE.RGBAFormat )
// //material.map = texture;
// //material.alphaMap = texture;

// digs = new THREE.Mesh( geometry, material );
// scene.add(digs);

// digs_neg = new THREE.Mesh( geometry, matdig_neg );
// scene.add(digs_neg);


/*
var size = 512*512*8*8;
var dataw = new Uint8Array( 4 * size );

for(var x = 0; x < wires.length; ++x){
    for(var y = 0; y < wires[x].length; ++y){
        adc = wires[x][y];
        var i = x+512*8*y;
        if(adc > 0) dataw[i*4+1] = 255;

        dataw[i*4+3] = Math.min(Math.abs(adc), 255); // transparent when zero
    }
}
*/

// used the buffer to create a DataTexture

// var texwire = new THREE.TextureLoader().load("wires.png");
// texwire.magFilter = THREE.NearestFilter;

// //var texwire = new THREE.DataTexture( dataw, 512*8, 512*8, THREE.RGBAFormat )
// var matwire = new THREE.MeshBasicMaterial( {color: 0x008000, side: THREE.DoubleSide, transparent: true} );
// //matwire.map = texwire;
// matwire.alphaMap = texwire;

// wire_mesh = new THREE.Mesh( geometry, matwire );
// scene.add(wire_mesh);


var mat_lin = new THREE.LineBasicMaterial( { color: 'gray'});

var mat_hit = new THREE.MeshBasicMaterial( { color: 0xffffff, side: THREE.DoubleSide } );

function TextureMaterial(fname, col){
    var mat = new THREE.MeshBasicMaterial( { color: col, side: THREE.DoubleSide, transparent: true, alphaTest: 1/512.} );
    var tex = new THREE.TextureLoader().load(fname, function(t){console.log('loaded', fname);}, undefined, function(err){console.log('error loading', fname, err);});
    tex.flipY = false; // some disagreement between conventions...
    tex.magFilter = THREE.NearestFilter;
    tex.minFilter = THREE.LinearFilter;
    //    tex.generateMipmaps = false;
    mat.alphaMap = tex;
    return mat;
}

function MakePlane(plane, name, outlines, digs, wires, hits){
    var N = plane.nwires;
    var pitch = plane.pitch;
    var tick_pitch = plane.tick_pitch;
    var cz = plane.center[2];
    var dz = pitch*N/2.;
    var ct = plane.center[0] + plane.nticks/2.*tick_pitch;
    var dt = plane.nticks/2.*plane.tick_pitch;

//    var group = new THREE.Group();

    // var geom = new THREE.Geometry();

    // geom.vertices.push(new THREE.Vector3( cz-dz, ct-dt, 0),
    //                    new THREE.Vector3( cz+dz, ct-dt, 0),
    //                    new THREE.Vector3( cz+dz, ct+dt, 0),
    //                    new THREE.Vector3( cz-dz, ct+dt, 0) );

//    geom.faces.push( new THREE.Face3( 0, 1, 2 ) );
//    geom.faces.push( new THREE.Face3( 1, 2, 3 ) );

//    geom.faceVertexUvs[0][0] = [[0, 0], [1, 0], [1, 1]];
//    geom.faceVertexUvs[0][1] = [[1, 0], [1, 1], [0, 1]];

//    outlines.add(new THREE.LineLoop( geom, mat_lin ));

    var geom = new THREE.PlaneBufferGeometry(dz*2, dt*2);




    for(var sign = -1; sign <= +1; sign += 2){
        var mat = TextureMaterial("digits/"+name+(sign < 0 ? "_neg" : "_pos")+".png",
                                  (sign < 0 ? 0x0000ff : 0xff0000));

        var d = new THREE.Mesh( geom, mat );
        d.position.set(cz, ct, 0);
        d.layers.set(plane.view);
        digs.add(d);
    }

    mat = TextureMaterial("wires/"+name+".png", 0x008000);
    var w = new THREE.Mesh( geom, mat );
    w.position.set(cz, ct, 0);
    w.layers.set(plane.view);
    wires.add(w);

    var edges = new THREE.EdgesGeometry( geom );
    var line = new THREE.LineSegments( edges, mat_lin );
    line.position.set(cz, ct, 0);
    line.layers.set(plane.view);
    outlines.add( line );

    for(idx in plane.hits){
        var hit = plane.hits[idx];

        geom = new THREE.PlaneBufferGeometry( plane.pitch*.9, plane.tick_pitch*.9);
        var h = new THREE.Mesh(geom, mat_hit);
        h.position.set(cz-dz + (hit.wire+.5)*plane.pitch,
                       ct-dt + (hit.tick+.5)*plane.tick_pitch,
                       1); // on top of the colours
        h.layers.set(plane.view);
        hits.add(h);
    }
}

var outlines = new THREE.Group();
var digs = new THREE.Group();
var wires = new THREE.Group();
var hits = new THREE.Group();

for(key in planes){
    MakePlane(planes[key], key, outlines, digs, wires, hits);
}

scene.add(outlines);
scene.add(digs);
scene.add(wires);
scene.add(hits);

//outlines[0].visible = false;
//outlines[1].visible = false;

var controls = new THREE.OrbitControls( camera, renderer.domElement );
//controls.target = group;
//controls.autoRotate = true;
//controls.enableDamping = true;
//camera.position.set( 0, 20, 100 );
camera.position.z = 500;

controls.target.set( 0, 0, 0 ); // view direction perpendicular to XY-plane
//controls.enableRotate = false;
controls.enableZoom = true;

controls.mouseButtons = {
    LEFT: THREE.MOUSE.PAN,
    MIDDLE: THREE.MOUSE.DOLLY,
    RIGHT: THREE.MOUSE.ROTATE
}

console.log(controls.touches);

// Seems to hang the touch controls entirely :(
//controls.touches = {
//    ONE: THREE.TOUCH.DOLLY_PAN,
//    TWO: THREE.TOUCH.ROTATE
//}

console.log(controls.touches);

controls.screenSpacePanning = true;

controls.update();

//camera.lookAt(mx, my, mz);

camera.layers.set(2); // only Z view

function animate() {
    requestAnimationFrame( animate );
//    group.rotation.x += 0.01;
//    group.rotation.y += 0.01;
//    cube2.rotation.z += 0.02;
//    camera.position.x += .01;
//    controls.update();
    renderer.render( scene, camera );
    return;
}

function Toggle(col, event, str){
    col.visible = !col.visible;
    event.target.innerHTML = (col.visible ? 'Hide ' : 'Show ')+str;
}

function ToggleRawDigits(event){Toggle(digs, event, 'RawDigits');}
function ToggleWires(event){Toggle(wires, event, 'Wires');}
function ToggleHits(event){Toggle(hits, event, 'Hits');}
function ToggleSpacePoints(event){Toggle(group, event, 'SpacePoints');}

wires.visible = false;
hits.visible = false;
group.visible = false;

function ZView(){camera.layers.set(2);}
function UView(){camera.layers.set(0);}
function VView(){camera.layers.set(1);}

function LightDark(event)
{
    if(document.body.style.backgroundColor != 'white'){
        newCol = 'white';
        event.target.innerHTML = 'Dark';
    }
    else{
        newCol = 'black'
        event.target.innerHTML = 'Light';
    }

    renderer.setClearColor(newCol);
    document.body.style.backgroundColor = newCol;

    for(let elem of document.querySelectorAll('button')){
        elem.style.backgroundColor = newCol;
    }

}

window.addEventListener( 'resize', onWindowResize, false );

function onWindowResize() {
    renderer.setSize( window.innerWidth, window.innerHeight-4);

    // Keep aspect ratio correct
    camera.top = -512*window.innerHeight/window.innerWidth;
    camera.bottom = +512*window.innerHeight/window.innerWidth;
    camera.updateProjectionMatrix();
}

window.addEventListener('unload', function(event) {
    renderer.dispose();
//    console.log( THREE.WebGLRenderer.info);
      });

animate();

console.log(renderer.info);

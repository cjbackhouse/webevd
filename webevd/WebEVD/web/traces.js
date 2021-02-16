const offset0 = 100;
const stride = 50;

fetch("evtid.json").then(resp => resp.json()).then(evtid => {
    document.getElementById('runbox').value = evtid.run;
    document.getElementById('subrunbox').value = evtid.subrun;
    document.getElementById('evtbox').value = evtid.evt;
});

document.OnKeyDown = function(evt)
{
    if(evt.keyCode == 13) { // enter
        window.location.href = "seek_traces/" + document.getElementById('runbox').value + '/' + document.getElementById('subrunbox').value + '/' + document.getElementById('evtbox').value;
    }
}

// Kick off the fetching of the JSONs
let dig_traces = fetch("dig_traces.json").then(response => response.json());
let wire_traces = fetch("wire_traces.json").then(response => response.json());
let hits = fetch("hits.json").then(response => response.json());

// This could all be done with CSS but the APIs for manipulating it don't look
// great
function GetCurrentVisibility(klass)
{
    for(let elem of document.getElementsByClassName(klass)){
        let vis = elem.getAttribute('visibility')
        return vis != 'hidden';
    }

    return false;// no elems at all
}

function SetVisibilityLabel(klass, state, button, str)
{
    window.sessionStorage[button.id] = state;

    for(let elem of document.getElementsByClassName(klass)){
        elem.setAttribute('visibility', state ? 'visible' : 'hidden');
    }
    // Tick and Cross emojis respectively
    button.innerHTML = (state ? '&#x2705 ' : '&#x274c ')+str;
}

function AddDropdownToggle(dropdown_id, klass, label, init = false)
{
    let tag = dropdown_id+'/'+label;
    if(window.sessionStorage[tag] != undefined){
        init = (window.sessionStorage[tag] == 'true');
    }

    let btn = document.createElement('button');
    btn.id = tag;
    SetVisibilityLabel(klass, init, btn, label);

    btn.addEventListener('click', function(){
        SetVisibilityLabel(klass, !GetCurrentVisibility(klass), btn, label);
    });

    document.getElementById(dropdown_id).appendChild(btn);
}

function new_svg_elem(name)
{
    return document.createElementNS('http://www.w3.org/2000/svg', name);
}

function EnsureSVGs(dict)
{
    let content = document.getElementById('content');
    document.getElementById('loading').style.display = 'none';

    for(let label in dict){
        for(let plane in dict[label]){
            if(document.getElementById(plane) != null) continue;

            let h2 = document.createElement('h2');
            h2.appendChild(document.createTextNode(plane));
            content.appendChild(h2);

            let svg = new_svg_elem('svg');
            svg.id = plane;
            content.appendChild(svg);
        } // end for plane
    } // end for label
}

async function handletraces(traces, tag, color, init)
{
    traces = await traces;

    EnsureSVGs(traces);

    for(let label in traces){
        for(let plane in traces[label]){
            let svg = document.getElementById(plane);

            let maxx = svg.getAttribute('width');
            let maxy = svg.getAttribute('height');

            // Collect all the svgs of the same label under one class
            let planegroup = new_svg_elem('g');
            planegroup.setAttribute('class', tag+'/'+label);

            for(let wireNo in traces[label][plane]){
                let wire = traces[label][plane][wireNo];

                let poly = new_svg_elem('polyline');
                poly.setAttribute('stroke', color);
                poly.setAttribute('fill', 'none');

                let pts = '-1,0 ';
                for(let t0 in wire){
                    let x = Math.floor(t0);
                    pts += (x-1)+',0 ';
                    for(let y of wire[t0]){
                        pts += x+','+(-y)+' ';
                        x += 1;
                        if(x > maxx) maxx = x;
                    }
                    pts += x+',0 ';
                }

                poly.setAttribute('points', pts);

                let trans = new_svg_elem('g');
                let offset = wireNo*stride + offset0;
                trans.setAttribute('transform', 'translate(0, '+offset+')');
                if(offset+stride > maxy) maxy = offset+stride;

                trans.appendChild(poly);
                planegroup.appendChild(trans);
            } // end for wire

            svg.setAttribute('width', maxx);
            svg.setAttribute('height', maxy);

            svg.appendChild(planegroup);
        } //end for plane

        AddDropdownToggle(tag+'_dropdown', tag+'/'+label, label, init);
    } // end for label
}

const kGausShape = [[-4.0, 0.0003],
                    [-3.0, 0.011],
                    [-2.5, 0.044],
                    [-2.0, 0.135],
                    [-1.5, 0.325],
                    [-1.0, 0.606],
                    [-0.5, 0.882],
                    [-0.25, 0.969],
                    [0, 1],
                    [+0.25, 0.969],
                    [+0.5, 0.882],
                    [+1.0, 0.606],
                    [+1.5, 0.325],
                    [+2.0, 0.135],
                    [+2.5, 0.044],
                    [+3.0, 0.011],
                    [+4.0, 0.0003]];

// Assign result so that the large traces json will be GC'd once we're done
// with it
async function handle_hits(hits)
{
    hits = await hits;
    EnsureSVGs(hits);

    for(let label in hits){
        for(let plane in hits[label]){
            let svg = document.getElementById(plane);

            let maxx = svg.getAttribute('width');
            let maxy = svg.getAttribute('height');

            // Collect all the svgs of the same label under one class
            let planegroup = new_svg_elem('g');
            planegroup.setAttribute('class', 'hits/'+label);

            for(let hit of hits[label][plane]){
                if(!isFinite(hit.tick) || !isFinite(hit.rms)) continue;

                let poly = new_svg_elem('polyline');
                poly.setAttribute('stroke', 'red');
                poly.setAttribute('fill', 'none');

                let pts = '';
                for(let p of kGausShape){
                    pts += (hit.tick+p[0]*hit.rms)+','+(-hit.peakamp*p[1])+' ';
                }

                if(hit.tick+4*hit.rms > maxx) maxx = hit.tick+4*hit.rms;

                poly.setAttribute('points', pts);

                let trans = new_svg_elem('g');
                let offset = offset0 + stride*hit.wire
                trans.setAttribute('transform', 'translate(0, '+offset+')');
                if(offset+stride > maxy) maxy = offset+stride;

                trans.appendChild(poly);
                planegroup.appendChild(trans);
            } // end for wire

            svg.setAttribute('width', maxx);
            svg.setAttribute('height', maxy);

            svg.appendChild(planegroup);
        } //end for plane

        AddDropdownToggle('hits_dropdown', 'hits/'+label, label, false);
    } // end for label
}

window.Theme = function(theme)
{
    document.body.className = theme;
    window.sessionStorage.theme = theme;
}


handletraces(dig_traces, 'digs', 'gray', false);
handletraces(wire_traces, 'wires', 'green', true);
handle_hits(hits);
dig_traces = wire_traces = hits = undefined; // allow large JSONs to be GC'd

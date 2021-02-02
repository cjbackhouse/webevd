fetch("evtid.json").then(resp => resp.json()).then(evtid => {
    document.getElementById('runbox').value = evtid.run;
    document.getElementById('subrunbox').value = evtid.subrun;
    document.getElementById('evtbox').value = evtid.evt;
});

document.OnKeyDown = function(evt)
{
    if(evt.keyCode == 13) { // enter
        window.location.href = "seek/" + document.getElementById('runbox').value + '/' + document.getElementById('subrunbox').value + '/' + document.getElementById('evtbox').value;
    }
}

// Kick off the fetching of the JSON
let dig_traces = fetch("dig_traces.json").then(response => response.json());

dig_traces.then(traces => {
    let content = document.getElementById('content');
    content.innerHTML = ''; // doesn't appear to be a cleaner way to clear
    for(let label in traces){
        let h1 = document.createElement('h1');
        h1.appendChild(document.createTextNode(label));
        content.appendChild(h1);

        for(let plane in traces[label]){
            let h2 = document.createElement('h2');
            h2.appendChild(document.createTextNode(plane));
            content.appendChild(h2);

            let svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
            svg.setAttribute('width', traces[label][plane][0].length);
            svg.setAttribute('height', 50*(traces[label][plane].length+3));

            let offset = 100;

            for(let wire of traces[label][plane]){
                let poly = document.createElementNS('http://www.w3.org/2000/svg', 'polyline');
                poly.setAttribute('stroke', 'gray');

                let yprev = -1e10;
                let x = 0;
                let pts = '';
                for(let y of wire){
                    if(y != yprev) pts += x+','+(offset-y)+' ';
                    yprev = y;
                    x += 1;
                }
                pts += x+','+(offset-yprev); // complete the line

                poly.setAttribute('points', pts);

                svg.appendChild(poly);
                
                offset += 50;
            } // end for wire

            content.appendChild(svg);
        } //end for plane
    } // end for label
});

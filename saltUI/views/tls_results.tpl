<!--
 *	
 * Copyright (c) 2016 Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
-->

% rebase('results.tpl', title='Page Title')

<script>
document.getElementById("navbar_tls").className = "active";
</script>

  <h3>TLS Classifier</h3>

  <table class="table table-striped">
    <thead>
      <tr>
        <th></th>
        <th>P(TLS)</th>
        <th>Source Address</th>
        <th>Destination Address</th>
        <th>Source Port</th>
        <th>Destination Port</th>
	<th>Inbound Bytes (Packets)</th>
	<th>Outbound Bytes (Packets)</th>
	<th>Protocol</th>
      </tr>
    </thead>
    <tbody>
    % for item in results:
      <tr>
        <td><div class="box" style="background-color:#{{item[9]}};"></div></td>
        <td><a data-toggle="modal" data-target="#basicModal" href="/advancedInfo/{{item[1].replace('.','')+item[2].replace('.','')+str(item[3])+str(item[4])+str(item[7]+item[8])}}">{{item[0]}}</a></td>
	% if item[10] == '':
        <td>{{item[1]}}</td>
	% else:
        <td>{{item[1]}} ({{item[10]}})</td>
	% end
	% if item[11] == '':
        <td>{{item[2]}}</td>
	% else:
        <td>{{item[2]}} ({{item[11]}})</td>
	% end
        <td>{{item[3]}}</td>
        <td>{{item[4]}}</td>
        <td>{{item[7]}} ({{item[5]}})</td>
        <td>{{item[8]}} ({{item[6]}})</td>
        <td>{{item[12]}}</td>
      </tr>
    % end
    </tbody>
  </table>

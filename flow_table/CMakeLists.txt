add_vpp_plugin(flow_table
	SOURCES
	vnetfilter/include/vnetfilter.h
	vnetfilter/include/hook.h
	vnetfilter/vnetfilter.c
	vnetfilter/node.c
	vnetfilter/hook.c
	vnetfilter/cli.c

	core/include/flow.h
	core/include/process.h
	core/include/protocol.h

	core/init.c
	core/flow.c
	core/protocol.c
	core/ip4_process.c
	core/ip6_process.c

	core/proto/unimplemented.c
	core/proto/tcp.c
	core/proto/tcp_state_transfer.h
	core/proto/udp.c
	core/proto/icmp.c
)

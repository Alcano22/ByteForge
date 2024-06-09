#pragma once

class Layer
{
public:
	Layer(const std::string& debugName = "Layer")
		: m_DebugName(debugName)
	{
	}
	
	virtual ~Layer() = default;

	virtual void OnAttach() {}
	virtual void OnDetach() {}
	virtual void Update(float deltaTime) {}
	virtual void Render() {}

	const std::string& GetName() const { return m_DebugName; }
protected:
	std::string m_DebugName;
};